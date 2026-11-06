#include "recovery.h"

#include "canvasview.h"
#include "fml_archive.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QHash>
#include <QHashFunctions>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStandardPaths>
#include <QString>
#include <Qt>
#include <QThread>
#include <QUuid>
#include <QtContainerFwd>
#include <QtTypes>

#include <memory>

#include "log/log.h"
#include "utils/utils.h"
using namespace familiar::log;

namespace familiar::recovery {
namespace {

QString id_stem(const QUuid& id)
{
    return id.toString(QUuid::WithoutBraces);
}

QString sidecar_path_for(const QDir& dir, const QUuid& id)
{
    return dir.filePath(id_stem(id) + QStringLiteral(".json"));
}

QString fml_path_for(const QDir& dir, const QUuid& id)
{
    return dir.filePath(id_stem(id) + QStringLiteral(".fml"));
}

// ── The background writer ────────────────────────────────────────────
// Reading a tab is 6ms on a 300-picture board; assembling its archive
// and writing it is 1.4s, all of it in memory and in one fdatasync'd
// write, and all of it used to run on the GUI thread every 30s. Only
// the read has to be there - see FmlArchive::capture()/write().
//
// Touched from the GUI thread only. The job posted by save() reads
// nothing from here: everything it needs is captured into the lambda.
struct Pending
{
    QPointer<CanvasView> view;
    // Identifies THIS job among every job ever queued for this tab.
    // Without it, a publish left over from an abandoned job
    // would be taken for the answer to the tab's next snapshot: it
    // would mark the tab saved at a revision that never reached disk,
    // and then the real answer, finding no entry of its own, would
    // delete the good file it had just written.
    quint64 id = 0;
};

struct WriterState
{
    QThread* thread = nullptr;
    // A plain QObject living on `thread`, there to be the target
    // posting a job at. ThreadedIO (fileio.h) cannot do this job: it
    // runs one function and exits, while this needs a thread whose
    // event loop outlives any single snapshot, so that snapshots of
    // different tabs queue behind each other instead of competing for
    // the same disk.
    QObject* context = nullptr;
    // Tabs whose content has been read and whose archive is queued or
    // being written, by recovery id. The QPointer is what tells a tab
    // that disappeared meanwhile apart from a live one: a snapshot
    // coming back for a tab that is no longer here must not be
    // published.
    QHash<QUuid, Pending> inFlight;
    quint64 nextJobId = 1;
    // Set by shutdown(). A QTimer can still fire between aboutToQuit
    // and the event loop actually stopping, and starting the thread
    // again there would write a snapshot into the folder clear() has
    // just wiped - which is the whole thing a clean exit is supposed to
    // leave empty.
    bool stopped = false;
};

WriterState& writer_state()
{
    static WriterState state;
    return state;
}

bool ensure_thread(WriterState& state)
{
    if (state.stopped) {
        return false;
    }
    if (!state.thread) {
        state.thread = new QThread();
        state.thread->setObjectName(QStringLiteral("recovery-snapshot"));
        state.context = new QObject();
        state.context->moveToThread(state.thread);
        state.thread->start();
    }
    return true;
}

// Waits for every snapshot already queued to finish writing, WITHOUT
// stopping the thread. shutdown() is the one that takes it down.
//
// Tearing the thread down here instead and letting the next save()
// start a fresh one was the first version. It worked, but it creates
// and joins an OS thread per clear(), and glibc reuses the pthread_t -
// which walks straight into a ThreadSanitizer runtime bug (a CHECK
// failure in its own thread registry, sanitizer_thread_registry.cpp),
// so the one tool that can actually check this code could not run on
// it. One long-lived thread costs nothing and keeps it checkable.
void flush(WriterState& state)
{
    if (!state.thread) {
        return;
    }

    // A no-op posted behind them. Jobs run in the order they were
    // queued, so this returns only once every one of them has - which
    // is the whole point: the caller is about to remove files, and a
    // write still running would put one back afterwards.
    QMetaObject::invokeMethod(
        state.context, [] {}, Qt::BlockingQueuedConnection);

    // Those jobs have posted their publishes, which have not been
    // delivered yet. Dropping the entries turns them into stale no-ops
    // (see Pending::id): the files they wrote are about to go, so there
    // is nothing left for them to settle.
    state.inFlight.clear();
}

// GUI thread, once the archive is on disk (or failed to get there).
void publish(const QUuid& id,
             quint64 jobId,
             quint64 revision,
             const FmlResult& result,
             const FmlSnapshot& snapshot)
{
    WriterState& state = writer_state();
    const auto it = state.inFlight.find(id);
    if (it == state.inFlight.end() || it->id != jobId) {
        // The answer to a job that flush() or shutdown() abandoned.
        // Either this tab has been snapshotted again since, and the
        // entry belongs to that newer job rather than to us, or it has
        // not, and the clear() that abandoned us has already removed
        // whatever this job wrote. Nothing to do either way.
        //
        // Keyed on the job id rather than on the entry simply being
        // gone, because a removed tab leaves its entry behind on
        // purpose (see remove()). Absence can then only mean an
        // abandoned job, and the two outcomes stop depending on which
        // answer happens to be delivered first.
        return;
    }

    const QPointer<CanvasView> view = it->view;
    state.inFlight.erase(it);

    // Before the error check below on purpose: the bytes are good
    // whether or not the archive reached the disk, and an encode nobody
    // keeps would be paid again on the next tick. A job that flush() or
    // shutdown() abandoned returns above without this - it has no view
    // to hand them to, and the cost is one re-encode.
    if (view) {
        FmlArchive::adopt_encoded_images(view->scene(), snapshot);
    }

    if (!result.error.isEmpty()) {
        // QSaveFile left whatever was there before untouched, and the
        // tab was never marked snapshotted, so the next tick retries.
        FLOG_WARN(Ch::IO,
                  "Could not write recovery snapshot {}: {}",
                  id_stem(id).toStdString(),
                  result.error.toStdString());
        return;
    }

    if (!view) {
        // remove() orphaned this job: the tab went away while this was
        // being written - closed, or discarded in the Save-All dialog -
        // and its files were deleted then. This write landed after
        // that, so take them out again instead of offering work the
        // user let go of back after the next crash.
        remove(id);
        return;
    }

    // `revision` is the one the tab was READ at, not the one it has
    // now: an edit made while this was being written leaves the tab due
    // another snapshot, which is exactly right - this archive does not
    // contain that edit.
    view->mark_recovery_snapshot_taken(revision);
}

} // namespace

QString recovery_dir()
{
    QString dir = portable_data_dir();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
    }
    return dir + QStringLiteral("/recovery");
}

void save(CanvasView* canvasView)
{
    WriterState& state = writer_state();
    const QUuid id = canvasView->recovery_id();
    // This tab's previous snapshot has not been written yet. Skipping
    // leaves it due one (nothing marks it taken until the file is
    // there), so the next tick picks it up - against queueing a second
    // archive of the same tab behind the first, which would only make
    // the queue, and the memory it holds, grow while the disk stays the
    // bottleneck.
    if (state.inFlight.contains(id) || !ensure_thread(state)) {
        return;
    }

    // Encode::Deferred: the encoding goes to the writer thread with
    // everything else. On the GUI thread this now only reads each
    // picture's pixmap, which is a refcounted shallow copy - see
    // FmlArchive::Encode.
    auto snapshot = std::make_shared<FmlSnapshot>();
    if (!FmlArchive::capture(canvasView->scene(),
                             canvasView->canvas_rect(),
                             *snapshot,
                             nullptr,
                             FmlArchive::Encode::Deferred)) {
        return;
    }
    const quint64 revision = canvasView->content_revision();

    const QDir dir(recovery_dir());
    QDir().mkpath(dir.absolutePath());

    const bool untitled = canvasView->is_untitled();
    QJsonObject sidecar;
    sidecar[QStringLiteral("originalPath")] = untitled ? QString()
                                                       : canvasView->path();
    sidecar[QStringLiteral("label")]
        = untitled ? QStringLiteral("Untitled (%1)")
                         .arg(QDateTime::currentDateTime().toString(
                             QStringLiteral("dd.MM HH:mm")))
                   : QFileInfo(canvasView->path()).fileName();

    const QString fmlPath = fml_path_for(dir, id);
    const QString sidecarPath = sidecar_path_for(dir, id);
    const QByteArray sidecarJson
        = QJsonDocument(sidecar).toJson(QJsonDocument::Compact);

    const quint64 jobId = state.nextJobId++;
    state.inFlight.insert(id, Pending{.view = canvasView, .id = jobId});

    // Everything below this line runs on the writer thread and reads
    // nothing but its own captures - no scene, no item, no QPixmap, no
    // WriterState. See FmlSnapshot (fml_archive.h).
    QMetaObject::invokeMethod(
        state.context,
        [id, jobId, revision, fmlPath, sidecarPath, sidecarJson, snapshot] {
            // Shared rather than moved through: write() records the
            // bytes it encoded in there, and publish() reads them back
            // on the GUI thread. The two lambdas never run at the same
            // time - this one posts that one.
            const FmlResult result = FmlArchive::write(*snapshot, fmlPath);

            // Only alongside an archive that made it: the sidecar
            // describes a file, and on its own it describes nothing.
            // Atomic, like the .fml next to it: this one is rewritten
            // on every snapshot while the app runs, so the write it has
            // to survive is the very crash the recovery folder exists
            // for. A truncated sidecar still parses as an empty object
            // (see scan() below), which costs originalPath - the
            // recovered file then shows up as "Untitled", unbound from
            // the file it came from.
            if (result.error.isEmpty()
                && !write_file_atomically(sidecarPath, sidecarJson)) {
                FLOG_WARN(Ch::IO,
                          "Could not write recovery sidecar {}",
                          sidecarPath.toStdString());
            }

            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [id, jobId, revision, result, snapshot] {
                    publish(id, jobId, revision, result, *snapshot);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void shutdown()
{
    WriterState& state = writer_state();
    // Terminal, unlike the flush() clear() does: nothing is accepted
    // afterwards.
    state.stopped = true;
    if (!state.thread) {
        return;
    }

    // quit() lets the snapshot being written right now finish and stops
    // the event loop then; the ones queued behind it are never started.
    // So this blocks for at most one snapshot, not for the whole queue -
    // which is what the way out wants, unlike clear()'s flush().
    state.thread->quit();
    state.thread->wait();

    // Safe to delete across threads now, and the only way left to: the
    // thread they belong to has finished, so deleteLater() would have
    // no event loop to run in.
    delete state.context;
    state.context = nullptr;
    delete state.thread;
    state.thread = nullptr;
    state.inFlight.clear();
}

void remove(const QUuid& id)
{
    // A snapshot of this tab that is queued or in flight cannot be
    // stopped, and its file must not reappear after the removals below.
    // So the entry stays and only loses its tab: publish() sees a job
    // with nowhere to go and deletes the file it just wrote. Keeping
    // the entry (rather than erasing it) is also what keeps
    // is_writing() true until that has actually happened.
    const auto it = writer_state().inFlight.find(id);
    if (it != writer_state().inFlight.end()) {
        it->view = nullptr;
    }

    const QDir dir(recovery_dir());
    QFile::remove(fml_path_for(dir, id));
    QFile::remove(sidecar_path_for(dir, id));
}

void clear()
{
    // Before the wipe, not after: a snapshot still being written would
    // otherwise land in the folder just after it was emptied, and show
    // up as recoverable work at the next launch - following a clean
    // exit, which is precisely what this function promises can't
    // happen.
    flush(writer_state());

    QDir dir(recovery_dir());
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

bool is_writing()
{
    return !writer_state().inFlight.isEmpty();
}

QList<Entry> scan()
{
    QList<Entry> entries;
    const QDir dir(recovery_dir());
    if (!dir.exists()) {
        return entries;
    }

    const QStringList fmlFiles = dir.entryList({QStringLiteral("*.fml")},
                                               QDir::Files);
    for (const QString& fmlFile : fmlFiles) {
        const QUuid id = QUuid::fromString(
            QFileInfo(fmlFile).completeBaseName());
        if (id.isNull()) {
            continue;
        }

        Entry entry;
        entry.id = id;
        entry.fmlPath = dir.filePath(fmlFile);

        QFile sidecarFile(sidecar_path_for(dir, id));
        if (sidecarFile.open(QIODevice::ReadOnly)) {
            const QJsonObject obj
                = QJsonDocument::fromJson(sidecarFile.readAll()).object();
            entry.originalPath
                = obj.value(QStringLiteral("originalPath")).toString();
            entry.label = obj.value(QStringLiteral("label")).toString();
        }
        if (entry.label.isEmpty()) {
            entry.label = entry.originalPath.isEmpty()
                              ? QStringLiteral("Untitled")
                              : QFileInfo(entry.originalPath).fileName();
        }

        entries.append(entry);
    }
    return entries;
}

} // namespace familiar::recovery

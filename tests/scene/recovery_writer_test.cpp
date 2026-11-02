#include "canvasscene.h"
#include "canvasview.h"
#include "commands.h"
#include "fml_archive.h"
#include "mainwindow.h"
#include "moveitem.h"
#include "recovery.h"
#include "tabpane.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QThread>
#include <QUndoStack>
#include <QUuid>

#include <functional>

// The recovery snapshot used to be assembled and written on the GUI
// thread: 1.4s of it on a 300-picture board, every 30 seconds. Reading
// the tab is 6ms of that and is the only part that has to be there, so
// the rest moved to a background thread (FmlArchive::capture()/write(),
// familiar::recovery::save()).
//
// What that buys is a freeze that no longer happens; what it costs is
// everything below - a file that lands after the tab may already have
// moved on, or been closed, or had its folder wiped. These cover the
// handover, not the gate that decides whether a snapshot is due at all
// (recovery_snapshot_test.cpp does that).
//
// Writing into the user's real recovery folder is kept off by
// QStandardPaths::setTestModeEnabled() in main()'s test branch.

namespace {

class RecoveryWriterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        familiar::recovery::clear();

        window_.tab_pane().add_new_untitled_tab();
        view_ = window_.tab_pane().current_widget();
        ASSERT_NE(view_, nullptr);
        id_ = view_->recovery_id();
    }

    void TearDown() override { familiar::recovery::clear(); }

    void push_an_edit()
    {
        auto* item = new PixmapItem(QImage(32, 24, QImage::Format_RGB32));
        view_->undo_stack()->push(
            new InsertItemsCommand(view_->scene(), {item}));
    }

    // Noise rather than a flat fill: a flat image compresses to almost
    // nothing, and it is the archive's SIZE that this is for.
    void push_pictures(int count, int side)
    {
        for (int i = 0; i < count; ++i) {
            QImage img(side, side, QImage::Format_RGB32);
            for (int y = 0; y < side; ++y) {
                auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < side; ++x) {
                    line[x] = qRgb((x * 7 + i) & 0xFF,
                                   (y * 13 + i) & 0xFF,
                                   (x * y + i) & 0xFF);
                }
            }
            auto* item = new PixmapItem(img);
            view_->undo_stack()->push(
                new InsertItemsCommand(view_->scene(), {item}));
        }
    }

    QString fml_path() const
    {
        return QDir(familiar::recovery::recovery_dir())
            .filePath(id_.toString(QUuid::WithoutBraces)
                      + QStringLiteral(".fml"));
    }

    QString sidecar_path() const
    {
        return QDir(familiar::recovery::recovery_dir())
            .filePath(id_.toString(QUuid::WithoutBraces)
                      + QStringLiteral(".json"));
    }

    // Waits on the writer's own state rather than on a duration: a
    // fixed sleep that turns out too short passes for the wrong reason
    // (nothing has been written yet, so of course nothing is wrong).
    static bool settle()
    {
        return pump_until([] { return !familiar::recovery::is_writing(); });
    }

    static bool pump_until(const std::function<bool()>& done)
    {
        constexpr qint64 ktimeoutMs = 30000;
        QElapsedTimer timer;
        timer.start();
        while (!done()) {
            if (timer.elapsed() > ktimeoutMs) {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            QThread::msleep(1);
        }
        return true;
    }

    MainWindow window_;
    CanvasView* view_ = nullptr;
    QUuid id_;
};

} // namespace

TEST_F(RecoveryWriterTest, SaveReturnsBeforeTheArchiveIsOnDisk)
{
    push_an_edit();
    ASSERT_TRUE(view_->needs_recovery_snapshot());

    familiar::recovery::save(view_);

    // The write happens on another thread and the tab is marked from a
    // queued event, so neither can have run before control came back
    // here - whatever the board's size. This is the whole change: the
    // call no longer waits for the disk.
    EXPECT_TRUE(familiar::recovery::is_writing());
    EXPECT_TRUE(view_->needs_recovery_snapshot());

    ASSERT_TRUE(settle());
    EXPECT_TRUE(QFile::exists(fml_path()));
    EXPECT_TRUE(QFile::exists(sidecar_path()));
    EXPECT_FALSE(view_->needs_recovery_snapshot());
}

// Against the measured cost of the same save done the old way, in this
// same process, rather than against a duration picked in advance: the
// point is not that save() is fast but that it no longer waits for the
// disk at all. A blocking handover would land squarely in between.
TEST_F(RecoveryWriterTest, SaveDoesNotWaitForTheDisk)
{
    push_pictures(80, 800);

    QTemporaryDir reference;
    ASSERT_TRUE(reference.isValid());
    const QString referencePath
        = reference.filePath(QStringLiteral("ref.fml"));

    // Twice, and only the second one timed. The first fills
    // PixmapItem's encode caches, and the difference between the two is
    // an order of magnitude - so timing the first would be comparing
    // the steady state against a cost nothing in the steady state pays,
    // and a save that still blocked would fit inside the margin.
    ASSERT_TRUE(FmlArchive::save(view_->scene(),
                                 view_->canvas_rect(),
                                 referencePath)
                    .error.isEmpty());
    QElapsedTimer timer;
    timer.start();
    const FmlResult result = FmlArchive::save(view_->scene(),
                                              view_->canvas_rect(),
                                              referencePath);
    const qint64 synchronousNs = timer.nsecsElapsed();
    ASSERT_TRUE(result.error.isEmpty()) << result.error.toStdString();

    constexpr qint64 kfloorNs = 25 * 1000 * 1000;
    if (synchronousNs < kfloorNs) {
        GTEST_SKIP() << "a synchronous save takes only "
                     << synchronousNs / 1000000 << "ms here - too little to "
                        "tell a blocking handover apart from a queued one";
    }

    timer.restart();
    familiar::recovery::save(view_);
    const qint64 queuedNs = timer.nsecsElapsed();

    EXPECT_LT(queuedNs * 5, synchronousNs)
        << "save() took " << queuedNs / 1000000 << "ms against "
        << synchronousNs / 1000000 << "ms for the same archive written "
        << "synchronously - it is still waiting for the write";

    ASSERT_TRUE(settle());
    EXPECT_TRUE(QFile::exists(fml_path()));
}

// capture() reads the items and write() assembles the archive from what
// it captured, with nothing of the scene in between. A field dropped on
// the way across would still produce a perfectly valid zip.
TEST_F(RecoveryWriterTest, TheArchiveItWritesLoadsBackWithItsItems)
{
    push_an_edit();
    push_an_edit();
    push_an_edit();
    familiar::recovery::save(view_);
    ASSERT_TRUE(settle());
    ASSERT_TRUE(QFile::exists(fml_path()));

    uint64_t zCounter = 0;
    QUndoStack undoStack;
    CanvasScene scene(window_, zCounter, &undoStack);
    const FmlResult result = FmlArchive::load(fml_path(), &scene);

    EXPECT_TRUE(result.error.isEmpty()) << result.error.toStdString();
    EXPECT_TRUE(result.itemErrors.isEmpty());
    EXPECT_EQ(scene.add_queued_items().size(), 3);
}

// A tab closed (or discarded in the Save-All dialog) while its snapshot
// was in flight. remove() has already deleted its files; the write
// lands after that, and what it leaves behind would be offered as
// recoverable work after the next crash - the one thing remove() exists
// to prevent.
TEST_F(RecoveryWriterTest, ATabRemovedWhileItsSnapshotIsInFlightLeavesNoFiles)
{
    push_an_edit();
    familiar::recovery::save(view_);
    familiar::recovery::remove(id_);

    // remove() leaves the job tracked, only with nowhere to deliver to.
    // Dropping it outright would make the wait below return before the
    // write had even started, and this test would then be asserting
    // that a file nobody has written yet does not exist.
    ASSERT_TRUE(familiar::recovery::is_writing());

    ASSERT_TRUE(settle());
    EXPECT_FALSE(QFile::exists(fml_path()));
    EXPECT_FALSE(QFile::exists(sidecar_path()));
}

// Same race against a clean exit, where the folder is wiped wholesale:
// after clear() there is nothing left to recover, and no snapshot still
// owing.
//
// It does NOT pin the ORDER inside clear(), and not for want of trying:
// waiting for the write after the wipe instead of before it passes this
// just the same, and so did every variant of it, because wiping takes
// the whole directory with it and a write landing afterwards then fails
// on its own (QSaveFile has nothing left to rename into). The wait is
// there to keep the guarantee local to clear() rather than resting on a
// syscall failing in a directory that is no longer there, and to leave
// is_writing() false - which this does pin, and which dropping the wait
// altogether breaks.
TEST_F(RecoveryWriterTest, ClearLeavesNothingBehindAndNothingOwing)
{
    push_an_edit();
    familiar::recovery::save(view_);
    ASSERT_TRUE(familiar::recovery::is_writing());

    familiar::recovery::clear();

    EXPECT_FALSE(familiar::recovery::is_writing());
    EXPECT_FALSE(QFile::exists(fml_path()));
    EXPECT_FALSE(QFile::exists(sidecar_path()));

    // And nothing arrives afterwards either - the drained job's answer
    // is stale by then and must not resurrect its file. If that job got
    // as far as writing, its answer was already posted by the time
    // clear() returned (it waits for the thread), so delivering what is
    // posted right now settles this without waiting on a duration.
    QCoreApplication::sendPostedEvents();
    EXPECT_FALSE(QFile::exists(fml_path()));
}

// clear() waits the writer out but does not retire it, so the next tick
// after one still gets its snapshot. shutdown() is the terminal one,
// and is deliberately not exercised here: it would stop the writer for
// the rest of the process, and every test after this one with it.
TEST_F(RecoveryWriterTest, TheWriterKeepsWorkingAfterAClear)
{
    push_an_edit();
    familiar::recovery::save(view_);
    familiar::recovery::clear();
    ASSERT_FALSE(QFile::exists(fml_path()));

    push_an_edit();
    familiar::recovery::save(view_);

    ASSERT_TRUE(settle());
    EXPECT_TRUE(QFile::exists(fml_path()));
    EXPECT_FALSE(view_->needs_recovery_snapshot());
}

// The archive lands at a revision the tab may already have left behind.
// save() hands the revision it READ at back to the tab, so an edit made
// while the write was running is not counted as having reached disk.
TEST_F(RecoveryWriterTest, AnEditDuringTheWriteLeavesTheTabDueAnotherSnapshot)
{
    push_an_edit();
    familiar::recovery::save(view_);

    // The user keeps working while the archive is being written. It
    // does not contain this.
    push_an_edit();

    ASSERT_TRUE(settle());
    EXPECT_TRUE(QFile::exists(fml_path()));
    EXPECT_TRUE(view_->needs_recovery_snapshot());
}

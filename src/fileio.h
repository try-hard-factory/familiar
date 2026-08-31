#ifndef FILEIO_H
#define FILEIO_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <functional>
#include <optional>

class CanvasScene;

// "Large" for Items/auto_optimize_imported_images purposes (warn /
// optimize_large): long side over this many pixels. Fixed, not itself a
// setting. Shared with canvasview.cpp's on_insert_images_finished(),
// which quotes it in the "large images imported" message. Unrelated to
// ImageLoadFailure::TooLarge below - that's about a load that FAILED
// outright (Items/image_allocation_limit, a memory ceiling in MB); this
// is a pixel-dimension threshold applied only AFTER a successful load.
constexpr int kLargeImageMaxDimension = 4096;

// Why ImageImportSession::run() couldn't produce a usable QImage for one url - lets
// the UI tell "this format isn't supported at all" apart from "this
// would need more memory than Items/image_allocation_limit allows" apart
// from "the file itself is corrupt/truncated", instead of one opaque
// "could not load". Qt's own QImageReader::error() can't fully make this
// distinction on its own (UnsupportedFormatError is real and distinct,
// but an allocation-limit rejection and genuine corrupt data both come
// back as the same InvalidDataError - see
// https://bugreports.qt.io/browse/QTBUG-124510) - TooLarge is
// disambiguated here by checking QImageReader::size() against the
// configured limit BEFORE attempting the real read().
enum class ImageLoadFailure {
    UnsupportedFormat,
    TooLarge,
    Corrupt,
};

// Dedicated thread for loading and saving.
class ThreadedIO : public QThread
{
    Q_OBJECT

public:
    using WorkerFunc = std::function<void(ThreadedIO*)>;

    explicit ThreadedIO(WorkerFunc func, QObject* parent = nullptr);

    std::atomic<bool> canceled{false};

    // QThread::msleep() is protected; expose it for worker functions
    // (which are not members of this class) the same way Python's
    // worker.msleep(10) is just called on the thread object directly.
    void sleepMs(unsigned long ms) { QThread::msleep(ms); }

protected:
    void run() override;

signals:
    void progress(int value);
    void finished(const QString& error, const QStringList& errors);
    void beginProcessing(int count);
    void userInputRequired(const QString& message);
    // Emitted once, right before finished(), when
    // ImageImportSession::run() ran in "warn" mode (Items/auto_optimize_imported_images)
    // and found one or more images over the large-image size threshold.
    // Separate from finished()'s `errors` - these images loaded fine and
    // were queued as-is, just flagged as candidates for optimization.
    void largeImagesFound(const QStringList& filenames);
    // Emitted once, right before finished(), by ImageImportSession::run() ONLY - the
    // 3-way breakdown of finished()'s own `errors` list (same filenames,
    // just classified - see ImageLoadFailure). finished()'s `errors`
    // keeps carrying the flat list too (log/back-compat), this is
    // additive for the UI to build a clearer message from.
    void imageLoadFailures(const QStringList& unsupportedFormat,
                          const QStringList& tooLarge,
                          const QStringList& corrupt);
    // Emitted (instead of finished()) by ImageImportSession::run() the
    // first time it reaches a RAW file whose handling isn't decided yet
    // (see ImageImportSession::setQueueChoice()) - the worker thread has
    // already stopped by the time this is delivered (run() just
    // returned, same pause/resume shape as userInputRequired() above/
    // ImagesToDirectoryExporter, export.h). The caller shows
    // RawImportDialog, calls setQueueChoice() on the SAME session
    // object, then start()s this same ThreadedIO again to resume from
    // exactly this file.
    void rawImportChoiceRequired(const QString& filename);

public slots:
    void onCanceled();

private:
    WorkerFunc func_;
};

// Recognized camera RAW extensions (case-insensitive) - LibRaw itself
// sniffs file content, not extension, but this app needs to decide
// BEFORE attempting a decode whether to route through LibRaw at all
// instead of QImageReader (and, if so, whether RawImportDialog needs to
// ask first) - not exhaustive (LibRaw supports far more obscure/legacy
// formats than this), covers current major camera manufacturers.
bool is_raw_file(const QString& filename);

enum class RawImportChoice {
    Optimize,     // fast: LibRaw's embedded JPEG preview, no demosaic
    KeepOriginal, // slow: full LibRaw demosaic (see canvasview's
                  // RawImportDialog for why "keep original" doesn't
                  // literally mean the source .NEF/.CR3 bytes - see its
                  // own doc comment)
};

// Adds images to an existing scene. Resumable, same shape as
// ImagesToDirectoryExporter (export.h): run() returns early (without
// reaching finished()) the first time it hits a RAW file whose handling
// isn't decided yet (see setQueueChoice()/ThreadedIO::
// rawImportChoiceRequired above) - the caller shows RawImportDialog,
// calls setQueueChoice(), and calls run() again (same worker,
// QThread::start() again) to resume from exactly that file.
//
// Each url is either a local file (QUrl::isLocalFile()), an embedded
// "data:" URI (decoded in-place, no network), or an http(s) URL
// (downloaded synchronously via QNetworkAccessManager + a nested
// QEventLoop - this runs on ThreadedIO's background thread, which has no
// event loop of its own to deliver QNetworkReply's signals otherwise).
// Queues the decoded QImage via CanvasScene::add_item_later()
// (mutex-protected, safe to call from any thread) — it never touches the
// QGraphicsScene itself. The caller is expected to drain the queue with
// CanvasScene::add_queued_items() on the GUI thread, e.g. from a slot
// connected to ThreadedIO::progress/finished (see CanvasView::do_insert_images).
class ImageImportSession
{
public:
    ImageImportSession(const QList<QUrl>& urls,
                       const QPointF& pos,
                       CanvasScene* scene);

    void run(ThreadedIO* worker);

    // Sticks for every remaining file in this session once set (mirrors
    // ExportImagesFileExistsDialog's skip_all/overwrite_all - "Apply
    // choice to this queue" checked in RawImportDialog).
    void setQueueChoice(RawImportChoice choice) { queueChoice_ = choice; }
    // Answers ONLY for pendingRawFile() (unchecked "Apply choice to this
    // queue") - consumed (cleared) the moment run() uses it, so the NEXT
    // RAW file in the queue, if any, pauses fresh again instead of
    // silently reusing this answer.
    void setOneShotChoice(RawImportChoice choice) { oneShotChoice_ = choice; }
    // Filename run() paused on - only meaningful right after
    // rawImportChoiceRequired() fired, before the next run().
    const QString& pendingRawFile() const { return pendingRawFile_; }

    // Accumulated across every run() call in this session (a pause+
    // resume does NOT reset these) - read once after finished() fires.
    QStringList unsupportedFormatErrors;
    QStringList tooLargeErrors;
    QStringList corruptErrors;
    QStringList largeImages;

private:
    QList<QUrl> urls_;
    QPointF pos_;
    CanvasScene* scene_;
    int nextIndex_ = 0;
    QString pendingRawFile_;
    std::optional<RawImportChoice> queueChoice_;
    std::optional<RawImportChoice> oneShotChoice_;
};

// Load familiar's native project file.
//
// TODOLATER: SQLiteIO-based read not ported (archive/manifest format not
// designed yet). Signature kept as in Python's load_bee(filename, scene,
// worker=None).
void load_fml(const QString& filename,
              CanvasScene* scene,
              ThreadedIO* worker = nullptr);

// Save familiar's native project file.
//
// TODOLATER: SQLiteIO-based write not ported (archive/manifest format not
// designed yet). Signature kept as in Python's
// save_bee(filename, scene, create_new=False, worker=None).
void save_fml(const QString& filename,
              CanvasScene* scene,
              bool createNew = false,
              ThreadedIO* worker = nullptr);

#endif // FILEIO_H

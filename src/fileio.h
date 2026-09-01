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
    // DIAG (ProgressDialog SIGSEGV investigation, see fileio.cpp): only
    // exists right now to log this object's address at destruction time,
    // for correlation against a crash backtrace's `this=` pointer.
    ~ThreadedIO() override;

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
    // Emitted right before ImageImportSession::run() starts decoding a
    // RAW file (either RawImportChoice - both go through a real LibRaw
    // demosaic now, see decode_raw_via_demosaic()), and again once that
    // decode returns (success or failure) - a full-resolution demosaic
    // specifically can take 10+ seconds with no incremental feedback
    // otherwise, so progress() alone would leave the progress bar
    // looking frozen/stuck for that whole stretch instead of showing
    // something's actively happening.
    void rawDecodeStateChanged(bool decoding);
    // Real sub-progress WITHIN a single RAW file's demosaic (either
    // RawImportChoice - Optimize's half_size=1 pass walks the same named
    // pipeline stages, just faster), 0-100 - LibRaw::
    // set_progress_handler() reports back which of its ~20 named
    // pipeline stages (OPEN, LOAD_RAW, INTERPOLATE, CONVERT_RGB, ...)
    // just completed; see fileio.cpp's rawProgressCallback().
    void rawDecodeProgress(int percent);
    // Name of the item about to be processed, emitted right before work
    // on it starts - purely for display (ProgressDialog shows it under
    // the operation's own title, so "Loading images" also says WHICH
    // image is loading right now). Optional: an operation that never
    // emits it just leaves that line blank.
    void currentItemChanged(const QString& name);

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
    // Both are a real LibRaw demosaic (decode_raw_via_demosaic(),
    // fileio.cpp) - Optimize used to just extract the camera's own
    // embedded preview JPEG instead (no demosaic at all), abandoned
    // after a real file's embedded preview turned out flat/unprocessed
    // (camera white balance/Picture Control never applied to it, unlike
    // what its own JPEG engine renders for a real photo) - confirmed by
    // dumping those exact bytes to disk and viewing them outside this
    // app entirely, not a decoding bug on this app's side.
    Optimize,     // fast: full resolution, but the cheapest real
                  // interpolation algorithm (user_qual=0, linear) rather
                  // than LibRaw's own best-for-this-camera default -
                  // NOT half_size (tried first, dropped: block-averaging
                  // instead of real interpolation produced a visible
                  // moire/noise pattern on flat surfaces)
    KeepOriginal, // slow: full-resolution demosaic (see canvasview's
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

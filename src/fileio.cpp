#include "fileio.h"

#include "canvasscene.h"
#include "core/settings.h"
#include "core/settingshandler.h"
#include "fml_archive.h"

#include <QBuffer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>

#include <optional>

#include <libraw/libraw.h>

#include "log/log.h"
using namespace familiar::log;

namespace {

// Google Images search results ("imgres?...") link to the search result
// page, not the image itself - the actual image URL is embedded in the
// "imgurl" query parameter. Unwrap it, same idea as unwrapping Pinterest
// page URLs to their real image target.
QUrl unwrap_known_redirect(const QUrl& url)
{
    if (url.host().endsWith(QStringLiteral("google.com"))
        && url.path().contains(QStringLiteral("imgres"))) {
        QUrlQuery query(url);
        QString imgUrl = query.queryItemValue(QStringLiteral("imgurl"),
                                              QUrl::FullyDecoded);
        if (!imgUrl.isEmpty()) {
            return QUrl(imgUrl);
        }
    }
    return url;
}

// Decoded image data plus the ENCODED bytes it came from - the latter is
// what an animated GIF actually needs (GifItem's QMovie decodes the
// whole stream itself; a single decoded QImage would just be its first
// frame). `image` alone is enough for anything else, so most call sites
// only ever look at that field.
struct LoadedImage
{
    QImage image;
    QByteArray bytes;
};

// LibRaw::set_progress_handler()'s callback is a plain C function
// pointer (no captures allowed), so context has to travel through its
// own `void* data` param instead of a lambda capture.
struct RawProgressContext
{
    ThreadedIO* worker;
};

// `stage` is always a single bit (LibRaw's own RUN_CALLBACK() call
// sites, libraw.h, each pass exactly one named LibRaw_progress flag per
// call) - OPEN(bit 0) through STRETCH(bit 19) are the ~20 real,
// meaningful stages a full demosaic actually walks through in order
// (LOAD_RAW/INTERPOLATE are typically the slow ones); bits 20+ are
// reserved/thumbnail-specific and don't fire here. [Непроверено] - the
// ORDER these fire in, and that there are exactly 20 of them for a
// typical file, is inferred from libraw_const.h's own enum declaration
// order, not observed against a real decode yet.
constexpr int kRawProgressKnownStages = 20;

int rawProgressCallback(void* data,
                        LibRaw_progress stage,
                        int /*iteration*/,
                        int /*expected*/)
{
    auto* ctx = static_cast<RawProgressContext*>(data);
    if (!ctx || !ctx->worker) {
        return 0;
    }
    unsigned bit = static_cast<unsigned>(stage);
    int bitPos = 0;
    while (bit > 1u) {
        bit >>= 1;
        ++bitPos;
    }
    if (bitPos < kRawProgressKnownStages) {
        const int percent = ((bitPos + 1) * 100) / kRawProgressKnownStages;
        emit ctx->worker->rawDecodeProgress(percent);
    }
    return 0; // non-zero would throw LIBRAW_EXCEPTION_CANCELLED_BY_CALLBACK
              // internally (libraw.h's RUN_CALLBACK macro) - not wired up
              // to worker->canceled here, out of scope for now.
}

// Real demosaic through LibRaw's own processing pipeline for BOTH
// RawImportChoice values - see the enum's own doc comment (fileio.h)
// for why RawImportChoice::Optimize used to just extract the camera's
// embedded preview JPEG instead, and why that was abandoned: on a real
// file, that embedded preview turned out to be a flat, unprocessed-
// looking render - camera settings (white balance/Picture Control) were
// never applied to it in the first place, confirmed by dumping those
// exact bytes to disk and looking at them in an ordinary image viewer,
// outside our code entirely.
//
// `fast` picks a quicker interpolation ALGORITHM (user_qual=0, linear -
// LibRaw's own doc: API-datastruct.html, "0-10: interpolation quality",
// 0 is the fastest real option) rather than downscaling resolution -
// first tried `half_size=1` instead (each 2x2 sensor block collapsed to
// one output pixel, skipping interpolation entirely), but that produced
// a real, visible moire/noise pattern on flat surfaces (a plain wall
// behind a subject, confirmed via a real screenshot) - block-averaging
// isn't real demosaicing, unlike even the cheapest proper interpolation
// mode. LibRaw's own default (user_qual left at its constructor value,
// -1 - "camera-specific best", src/utils/init_close_utils.cpp) is used
// for RawImportChoice::KeepOriginal, unchanged.
//
// White balance is deliberately left at LibRaw's OWN defaults - its
// constructor (same file) sets output_color=1 (sRGB) and
// use_camera_matrix=1 but leaves use_camera_wb/use_auto_wb unset, so the
// render uses the camera profile's daylight-referred multipliers.
// use_camera_wb=1 was tried here and REVERTED: the camera's recorded
// white balance is what NEUTRALIZES the light it was shot under, so
// applying it to a warm-lit scene renders it flat and grey - visibly
// wrong next to how the same file looks in PureRef, and next to what
// this same code produced before that flag was added (both warm; both
// confirmed against real screenshots of the same .NEF). Leaving WB
// alone is what actually matches.
QImage decode_raw_via_demosaic(const QString& filename,
                               ThreadedIO* worker,
                               bool fast)
{
    LibRaw processor;
    RawProgressContext progressCtx{worker};
    processor.set_progress_handler(&rawProgressCallback, &progressCtx);
    // processor.imgdata.params.use_camera_wb = 1;
    if (fast) {
        processor.imgdata.params.user_qual = 0;
    }
    if (processor.open_file(filename.toLocal8Bit().constData())
        != LIBRAW_SUCCESS) {
        return {};
    }
    if (processor.unpack() != LIBRAW_SUCCESS) {
        return {};
    }
    if (processor.dcraw_process() != LIBRAW_SUCCESS) {
        return {};
    }
    int err = 0;
    libraw_processed_image_t* image = processor.dcraw_make_mem_image(&err);
    if (!image) {
        return {};
    }
    QImage result;
    if (image->type == LIBRAW_IMAGE_BITMAP && image->colors == 3
        && image->bits == 8) {
        result = QImage(reinterpret_cast<const uchar*>(image->data),
                        image->width,
                        image->height,
                        image->width * 3,
                        QImage::Format_RGB888)
                     .copy();
    }
    LibRaw::dcraw_clear_mem(image);
    return result;
}

QImage decode_raw(const QString& filename, RawImportChoice choice, ThreadedIO* worker)
{
    return decode_raw_via_demosaic(filename,
                                   worker,
                                   choice == RawImportChoice::Optimize);
}

bool is_image_large(const QImage& img)
{
    return img.width() > kLargeImageMaxDimension
           || img.height() > kLargeImageMaxDimension;
}

// Classifies WHY `reader` can't produce an image, without attempting the
// real (possibly expensive) decode itself - see ImageLoadFailure's own
// comment (fileio.h) for why this needs its own check rather than
// trusting QImageReader::error() alone. `allocationLimitBytes <= 0`
// means "no limit" (matches Items/image_allocation_limit's own "0 = no
// limitation" convention - see setting_descriptions.cpp), so TooLarge
// can never fire in that case.
//
// Only UnsupportedFormat/TooLarge come back from here - a reader that
// passes both these checks might still fail its real read() afterward
// (genuinely corrupt/truncated data); the caller is expected to fall
// back to ImageLoadFailure::Corrupt itself in that case, since this
// function never performs the real decode.
std::optional<ImageLoadFailure> precheckReader(QImageReader& reader,
                                               qint64 allocationLimitBytes)
{
    if (!reader.canRead()) {
        return ImageLoadFailure::UnsupportedFormat;
    }
    const QSize size = reader.size();
    if (allocationLimitBytes > 0 && size.isValid()) {
        // Same "32 bits per pixel minimum" accounting Qt's own
        // setAllocationLimit() documents itself - matches what actually
        // gets rejected internally, not just a rough estimate.
        const qint64 required = qint64(size.width()) * size.height() * 4;
        if (required > allocationLimitBytes) {
            return ImageLoadFailure::TooLarge;
        }
    }
    return std::nullopt;
}

// Runs precheckReader() over whatever bytes a failed load actually
// produced (populated regardless of success by every ImageImportSession::run()
// branch - local file/data: URL/download alike), so a single
// classification path covers all three sources without threading
// QImageReader through each of them individually. Empty `bytes` (e.g. a
// download that never got any data back at all) falls back to Corrupt -
// the real reason is already in the network-layer FLOG_WARN from
// download_image() above.
ImageLoadFailure classifyFailedLoad(const QByteArray& bytes,
                                    qint64 allocationLimitBytes)
{
    if (bytes.isEmpty()) {
        return ImageLoadFailure::Corrupt;
    }
    QBuffer buf;
    buf.setData(bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    if (auto failure = precheckReader(reader, allocationLimitBytes)) {
        return *failure;
    }
    return ImageLoadFailure::Corrupt;
}

// Scales `img` down in place to fit within kLargeImageMaxDimension on its
// longer side, preserving aspect ratio. No-op if already within bounds.
void downscale_to_limit(QImage& img)
{
    if (!is_image_large(img)) {
        return;
    }
    img = img.scaled(kLargeImageMaxDimension,
                     kLargeImageMaxDimension,
                     Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);
}

// True if `bytes` is a multi-frame (animated) image Qt can decode -
// checked on the raw bytes regardless of where they came from (local
// file/data URI/download), so all three sources get the same treatment.
bool is_animated(const QByteArray& bytes)
{
    if (bytes.isEmpty()) {
        return false;
    }
    QBuffer buf;
    buf.setData(bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    return reader.imageCount() > 1;
}

// Decodes an embedded "data:[<mediatype>][;base64],<data>" URI - no
// network involved. Percent-decoding the payload before base64-decoding
// it is a no-op if the base64 alphabet survived QUrl's own encoding
// unchanged, and correctly undoes it if QUrl percent-encoded any of
// '+'/'/'/'=' along the way - safe either way.
LoadedImage decode_data_url(const QUrl& url)
{
    QString full = url.toString(QUrl::FullyEncoded);
    if (!full.startsWith(QStringLiteral("data:"))) {
        return {};
    }
    QString payload = full.mid(5);
    int commaIdx = payload.indexOf(QLatin1Char(','));
    if (commaIdx < 0) {
        return {};
    }
    QString header = payload.left(commaIdx);
    QByteArray decoded = QByteArray::fromPercentEncoding(
        payload.mid(commaIdx + 1).toLatin1());

    QByteArray bytes = header.contains(QStringLiteral("base64"),
                                       Qt::CaseInsensitive)
                           ? QByteArray::fromBase64(decoded)
                           : decoded;

    QImage img;
    img.loadFromData(bytes);
    return {img, bytes};
}

constexpr int kDownloadTimeoutMs = 15000;
constexpr int kCancelPollMs = 200;

// Blocking download: ImageImportSession::run() runs on ThreadedIO's background
// thread, which has no event loop of its own to deliver
// QNetworkAccessManager's normally-async signals - a nested QEventLoop
// here blocks until this one reply completes, standard Qt idiom for
// "synchronous" network requests off the GUI thread.
//
// An unreachable/slow host would otherwise hang this loop (and the whole
// ImageImportSession::run() run, and its progress dialog) forever - a timeout, plus
// polling `worker->canceled` so the dialog's own Cancel button actually
// works mid-download, both just quit the same local loop early.
LoadedImage download_image(QNetworkAccessManager& manager,
                           const QUrl& url,
                           ThreadedIO* worker)
{
    QNetworkRequest request(url);
    // Some CDNs/anti-bot setups throttle or stall the response body for
    // requests with no recognizable browser User-Agent (observed: fast
    // headers, then the transfer stalls) - claim to be an ordinary
    // browser to avoid that.
    request
        .setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral(
                       "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                       "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(kDownloadTimeoutMs);

    QTimer cancelPollTimer;
    QObject::connect(&cancelPollTimer,
                     &QTimer::timeout,
                     &loop,
                     [&loop, worker]() {
                         if (worker->canceled) {
                             loop.quit();
                         }
                     });
    cancelPollTimer.start(kCancelPollMs);

    loop.exec();

    QImage img;
    QByteArray bytes;
    if (!reply->isFinished()) {
        // Distinguishable via worker->canceled - a user-pressed Cancel is
        // routine, not a failure; a real timeout (nothing else quit the
        // loop) is worth a WARN.
        if (worker->canceled) {
            FLOG_DEBUG(Ch::Net, "Download canceled by user: {}", url.toString());
        } else {
            FLOG_WARN(Ch::Net, "Download timed out: {}", url.toString());
        }
        reply->abort();
    } else if (reply->error() == QNetworkReply::NoError) {
        bytes = reply->readAll();
        img.loadFromData(bytes);
    } else {
        FLOG_WARN(Ch::Net, "Downloading image failed: {}", reply->errorString());
    }
    reply->deleteLater();
    return {img, bytes};
}

} // namespace

// ─── ThreadedIO ────────────────────────────────────────────────────────────

ThreadedIO::ThreadedIO(WorkerFunc func, QObject* parent)
    : QThread(parent)
    , func_(std::move(func))
{
    // DIAG (ProgressDialog SIGSEGV investigation): correlate this
    // object's address across its whole lifetime against whatever
    // ProgressDialog logs alongside it, and against any later crash's
    // `this=` pointer in a backtrace.
    FLOG_DEBUG(Ch::IO, "ThreadedIO() this={}", static_cast<void*>(this));
}

ThreadedIO::~ThreadedIO()
{
    // DIAG: if a crash's backtrace pointer matches an address logged
    // here as already destroyed, that's direct proof of a stale/
    // use-after-free delivery rather than a same-object re-entry.
    FLOG_DEBUG(Ch::IO, "~ThreadedIO() this={}", static_cast<void*>(this));
}

void ThreadedIO::run()
{
    FLOG_DEBUG(Ch::IO,
              "ThreadedIO::run() this={} thread={}",
              static_cast<void*>(this),
              static_cast<void*>(QThread::currentThreadId()));
    func_(this);
}

void ThreadedIO::onCanceled()
{
    canceled = true;
}

bool is_raw_file(const QString& filename)
{
    static const QSet<QString> kRawExtensions = {
        QStringLiteral("nef"), QStringLiteral("nrw"), // Nikon
        QStringLiteral("cr2"), QStringLiteral("cr3"),
        QStringLiteral("crw"), // Canon
        QStringLiteral("arw"), QStringLiteral("srf"),
        QStringLiteral("sr2"), // Sony
        QStringLiteral("orf"), // Olympus
        QStringLiteral("raf"), // Fujifilm
        QStringLiteral("rw2"), // Panasonic/Leica
        QStringLiteral("pef"), QStringLiteral("ptx"), // Pentax
        QStringLiteral("srw"), // Samsung
        QStringLiteral("dng"), // Adobe DNG (any manufacturer)
        QStringLiteral("x3f"), // Sigma (Foveon)
        QStringLiteral("3fr"), // Hasselblad
        QStringLiteral("iiq"), // Phase One
        QStringLiteral("dcr"), QStringLiteral("kdc"), // Kodak
        QStringLiteral("mos"), // Leaf
        QStringLiteral("erf"), // Epson
        QStringLiteral("mef"), // Mamiya
    };
    return kRawExtensions.contains(QFileInfo(filename).suffix().toLower());
}

// ─── ImageImportSession ─────────────────────────────────────────────────────

ImageImportSession::ImageImportSession(const QList<QUrl>& urls,
                                       const QPointF& pos,
                                       CanvasScene* scene)
    : urls_(urls)
    , pos_(pos)
    , scene_(scene)
{}

void ImageImportSession::run(ThreadedIO* worker)
{
    // DIAG (ProgressDialog SIGSEGV investigation): confirms exactly how
    // many urls this session holds and where it's resuming from - a
    // progress(N) with N >= 1 only makes sense if urls_.size() > 1, or
    // if this is genuinely a second run() call resuming past index 0.
    FLOG_DEBUG(Ch::IO,
              "ImageImportSession::run() this={} worker={} urls={} "
              "nextIndex={}",
              static_cast<void*>(this),
              static_cast<void*>(worker),
              urls_.size(),
              nextIndex_);
    // Only on the very first call, not a resume after
    // rawImportChoiceRequired() - re-emitting this would reset the
    // ProgressDialog's bar back to 0, even though everything before
    // nextIndex_ already loaded.
    if (nextIndex_ == 0) {
        emit worker->beginProcessing(urls_.size());
    }

    const QString optimizeMode
        = SettingsHandler::getInstance()->autoOptimizeImportedImages();
    // 0 = no limitation (Items/image_allocation_limit's own convention -
    // see setting_descriptions.cpp) - precheckReader()/classifyFailedLoad()
    // both already treat <= 0 as "never TooLarge".
    const qint64 allocationLimitBytes
        = qint64(FamSettings()
                     .valueOrDefault(QStringLiteral("Items/image_allocation_limit"))
                     .toInt())
          * 1024 * 1024;
    const QString rawImportSetting
        = FamSettings()
              .valueOrDefault(QStringLiteral("Items/raw_import_choice"))
              .toString();

    // Only constructed if actually needed (most drops are local files).
    QNetworkAccessManager* netManager = nullptr;

    for (int i = nextIndex_; i < urls_.size(); ++i) {
        const QUrl& rawUrl = urls_.at(i);
        QImage img;
        QByteArray bytes;
        QString label;
        // Whether this url actually went through the LibRaw path below
        // (not just "its name ends in .nef") - see the large-image
        // handling further down for what it suppresses.
        bool decodedAsRaw = false;

        if (rawUrl.isLocalFile()) {
            label = rawUrl.toLocalFile();

            if (is_raw_file(label)) {
                decodedAsRaw = true;
                RawImportChoice choice;
                if (rawImportSetting == QLatin1String("always_optimize")) {
                    choice = RawImportChoice::Optimize;
                } else if (rawImportSetting
                          == QLatin1String("always_keep_original")) {
                    choice = RawImportChoice::KeepOriginal;
                } else if (queueChoice_) {
                    choice = *queueChoice_;
                } else if (oneShotChoice_) {
                    choice = *oneShotChoice_;
                    oneShotChoice_.reset();
                } else {
                    // Pause here - the caller shows RawImportDialog,
                    // calls setQueueChoice(), and calls run() again
                    // (same ThreadedIO, restarted) to resume from this
                    // exact file - see ThreadedIO::
                    // rawImportChoiceRequired's own doc comment.
                    FLOG_DEBUG(Ch::IO,
                              "Pausing for RAW import choice: {}",
                              label);
                    pendingRawFile_ = label;
                    nextIndex_ = i;
                    delete netManager;
                    emit worker->rawImportChoiceRequired(label);
                    return;
                }
                FLOG_DEBUG(Ch::IO,
                          "Decoding RAW file ({}) {}",
                          choice == RawImportChoice::Optimize
                              ? "fast half-size demosaic"
                              : "full-resolution demosaic",
                          label);
                emit worker->rawDecodeStateChanged(true);
                img = decode_raw(label, choice, worker);
                emit worker->rawDecodeStateChanged(false);
                // No raw-byte preservation (Max's explicit call - see
                // ImageImportSession's own doc comment in fileio.h) -
                // `bytes` stays empty, same as any decode failure below;
                // is_animated(empty) already safely returns false, so
                // this reads as an ordinary non-animated image to
                // everything downstream.
            } else {
                FLOG_DEBUG(Ch::IO, "Loading image from file {}", label);
                // Raw bytes read unconditionally (cheap - these are
                // images, not video), not just the decoded first frame:
                // an animated GIF needs the whole stream for GifItem's
                // QMovie, and is_animated() below needs something to
                // sniff regardless.
                QFile file(label);
                if (file.open(QIODevice::ReadOnly)) {
                    bytes = file.readAll();
                }
                QImageReader reader(label);
                reader.setAutoTransform(true); // apply EXIF rotation
                img = reader.read();
            }
        } else if (rawUrl.scheme().compare(QStringLiteral("data"),
                                           Qt::CaseInsensitive)
                   == 0) {
            label = rawUrl.toString();
            FLOG_DEBUG(Ch::IO, "Decoding embedded data: image");
            LoadedImage loaded = decode_data_url(rawUrl);
            img = loaded.image;
            bytes = loaded.bytes;
        } else {
            QUrl url = unwrap_known_redirect(rawUrl);
            label = url.toString();
            if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive)
                    == 0
                || url.scheme().compare(QStringLiteral("https"),
                                        Qt::CaseInsensitive)
                       == 0) {
                FLOG_DEBUG(Ch::Net, "Downloading image from {}", label);
                if (!netManager) {
                    netManager = new QNetworkAccessManager();
                }
                LoadedImage loaded = download_image(*netManager, url, worker);
                img = loaded.image;
                bytes = loaded.bytes;
            } else {
                FLOG_DEBUG(Ch::IO, "Unsupported URL scheme: {}", label);
            }
        }

        // DIAG (ProgressDialog SIGSEGV investigation): every progress()
        // emission's worker pointer + thread, to line up against
        // ProgressDialog's own DIAG logs and any crash backtrace.
        FLOG_DEBUG(Ch::IO,
                  "emit progress({}) worker={} thread={}",
                  i,
                  static_cast<void*>(worker),
                  static_cast<void*>(QThread::currentThreadId()));
        emit worker->progress(i);

        if (img.isNull()) {
            const ImageLoadFailure failure
                = classifyFailedLoad(bytes, allocationLimitBytes);
            switch (failure) {
            case ImageLoadFailure::UnsupportedFormat:
                FLOG_WARN(Ch::IO, "Unsupported image format: {}", label);
                unsupportedFormatErrors.append(label);
                break;
            case ImageLoadFailure::TooLarge:
                FLOG_WARN(Ch::IO,
                          "Image exceeds the {} MB allocation limit: {}",
                          allocationLimitBytes / (1024 * 1024),
                          label);
                tooLargeErrors.append(label);
                break;
            case ImageLoadFailure::Corrupt:
                FLOG_WARN(Ch::IO, "Could not load (corrupt file?): {}", label);
                corruptErrors.append(label);
                break;
            }
            continue;
        }

        // Items/auto_optimize_imported_images, skipped entirely for two
        // kinds of import:
        //
        // - an animated GIF: GifItem plays back the raw `bytes` stream
        //   via QMovie, not the decoded first-frame `img` below, so
        //   shrinking `img` alone wouldn't touch what's actually shown.
        // - a RAW file: this setting's whole job (warn about, or
        //   automatically shrink, oversized imports) is already covered
        //   for RAW by RawImportDialog, which asked about THIS specific
        //   file, by name, moments earlier - so letting this run on top
        //   would either nag on literally every RAW import (a modern
        //   sensor exceeds kLargeImageMaxDimension essentially by
        //   definition) or silently shrink a file the user just
        //   explicitly chose "Keep original" for. RawImportChoice is the
        //   answer for RAW; this setting governs everything else.
        const bool animated = is_animated(bytes);
        if (!animated && !decodedAsRaw) {
            if (optimizeMode == QLatin1String("warn")) {
                if (is_image_large(img)) {
                    FLOG_DEBUG(Ch::IO, "{} is a large image", label);
                    largeImages.append(label);
                }
            } else if (optimizeMode == QLatin1String("optimize_large")) {
                downscale_to_limit(img);
            }
        }

        // Queue the raw data instead of constructing a Pixmap/GifItem
        // here: this runs on a background thread, and
        // add_queued_items() (the consumer) must only ever touch the
        // QGraphicsScene from the GUI thread. Center the item on `pos`,
        // matching what PixmapItem::set_pos_center() would do for a
        // fresh item (scale 1, rotation 0). Computed after the possible
        // downscale above so the item is centered on its final size.
        QPointF topLeft = pos_ - QPointF(img.width() / 2.0, img.height() / 2.0);

        QVariantMap itemData;
        if (animated) {
            FLOG_DEBUG(Ch::IO, "{} is animated - loading as GifItem", label);
            itemData[QStringLiteral("type")] = QStringLiteral("gif");
            itemData[QStringLiteral("gifBytes")] = bytes;
        } else {
            itemData[QStringLiteral("type")] = QStringLiteral("pixmap");
            itemData[QStringLiteral("image")] = img;
        }
        itemData[QStringLiteral("filename")] = label;
        itemData[QStringLiteral("x")] = topLeft.x();
        itemData[QStringLiteral("y")] = topLeft.y();
        scene_->add_item_later(itemData, true);

        if (worker->canceled) {
            break;
        }
        worker->sleepMs(10);
    }

    delete netManager;

    if (!largeImages.isEmpty()) {
        emit worker->largeImagesFound(largeImages);
    }
    if (!unsupportedFormatErrors.isEmpty() || !tooLargeErrors.isEmpty()
        || !corruptErrors.isEmpty()) {
        emit worker->imageLoadFailures(unsupportedFormatErrors,
                                       tooLargeErrors,
                                       corruptErrors);
    }
    // Flat list for finished()'s own `errors` param - unchanged shape for
    // whatever else still just wants "what failed", the 3-way breakdown
    // above is additive, not a replacement.
    const QStringList errors
        = unsupportedFormatErrors + tooLargeErrors + corruptErrors;
    // DIAG (ProgressDialog SIGSEGV investigation).
    FLOG_DEBUG(Ch::IO, "emit finished() worker={}", static_cast<void*>(worker));
    emit worker->finished(QString(), errors);
    worker->quit();
}

// ─── load_fml / save_fml ───────────────────────────────────────────────────

void load_fml(const QString& filename, CanvasScene* scene, ThreadedIO* worker)
{
    FLOG_DEBUG(Ch::IO, "Loading from file {} ...", filename);

    FmlResult result = FmlArchive::load(filename, scene, worker);

    if (worker) {
        emit worker->finished(result.error, result.itemErrors);
        worker->quit();
    }
}

void save_fml(const QString& filename,
              CanvasScene* scene,
              bool createNew,
              ThreadedIO* worker)
{
    FLOG_DEBUG(Ch::IO, "Saving to file {} ...", filename);
    FLOG_DEBUG(Ch::IO, "Create new: {}", createNew);
    // The zip format always rewrites the whole archive - there's no
    // incremental-update concept for createNew=false to opt out of (see
    // docs/fml_format_design.md §2/§8).
    Q_UNUSED(createNew)

    // TODOLATER: no CanvasView here to read canvasRect() from (this
    // wrapper isn't actually called anywhere yet - see
    // FileActions::saveFile(), which calls FmlArchive::save() directly
    // instead so it can pass the real one). scene->rememberedBoundingRect()
    // is a reasonable stand-in once this does get wired up, but it's
    // really meant as a one-shot value read right after a load, not an
    // ongoing substitute for the view's own canvasRect().
    FmlResult result = FmlArchive::save(scene,
                                        scene->rememberedBoundingRect(),
                                        filename,
                                        worker);

    if (worker) {
        emit worker->finished(result.error, result.itemErrors);
        worker->quit();
    }
    FLOG_DEBUG(Ch::IO, "End save");
}

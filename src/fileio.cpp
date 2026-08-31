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

// Fast path (RawImportChoice::Optimize): LibRaw's own embedded JPEG/
// bitmap preview, no demosaic at all. Most modern cameras embed a
// near-full-resolution preview - plenty for a reference board, which
// has no RAW-specific editing (white balance/exposure/etc) that would
// actually need the real sensor data. [Непроверено] - written against
// LibRaw's documented C++ API (open_file/unpack_thumb/
// dcraw_make_mem_thumb/dcraw_clear_mem), not yet exercised against a
// real .NEF/.CR3 file.
QImage decode_raw_preview(const QString& filename)
{
    LibRaw processor;
    if (processor.open_file(filename.toLocal8Bit().constData())
        != LIBRAW_SUCCESS) {
        return {};
    }
    if (processor.unpack_thumb() != LIBRAW_SUCCESS) {
        return {};
    }
    int err = 0;
    libraw_processed_image_t* thumb = processor.dcraw_make_mem_thumb(&err);
    if (!thumb) {
        return {};
    }
    QImage result;
    if (thumb->type == LIBRAW_IMAGE_JPEG) {
        result.loadFromData(reinterpret_cast<const uchar*>(thumb->data),
                            static_cast<int>(thumb->data_size));
    } else if (thumb->type == LIBRAW_IMAGE_BITMAP && thumb->colors == 3
               && thumb->bits == 8) {
        // Detach (.copy()) BEFORE dcraw_clear_mem() below frees thumb->data
        // out from under a QImage that would otherwise just be wrapping it.
        result = QImage(reinterpret_cast<const uchar*>(thumb->data),
                        thumb->width,
                        thumb->height,
                        thumb->width * 3,
                        QImage::Format_RGB888)
                     .copy();
    }
    LibRaw::dcraw_clear_mem(thumb);
    return result;
}

// Slow path (RawImportChoice::KeepOriginal): a real demosaic through
// LibRaw's own default processing pipeline. Best available quality, but
// can take real time (seconds, not milliseconds) on a modern
// high-megapixel sensor - see RawImportDialog/ProgressDialog on the
// caller side. [Непроверено] - same caveat as decode_raw_preview() above.
QImage decode_raw_full(const QString& filename)
{
    LibRaw processor;
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

QImage decode_raw(const QString& filename, RawImportChoice choice)
{
    return choice == RawImportChoice::Optimize ? decode_raw_preview(filename)
                                               : decode_raw_full(filename);
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
{}

void ThreadedIO::run()
{
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

        if (rawUrl.isLocalFile()) {
            label = rawUrl.toLocalFile();

            if (is_raw_file(label)) {
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
                              ? "preview"
                              : "full demosaic",
                          label);
                img = decode_raw(label, choice);
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

        // Items/auto_optimize_imported_images: an animated GIF is
        // excluded either way - GifItem plays back the raw `bytes`
        // stream via QMovie, not the decoded first-frame `img` below, so
        // shrinking `img` alone wouldn't touch what's actually displayed.
        const bool animated = is_animated(bytes);
        if (!animated) {
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

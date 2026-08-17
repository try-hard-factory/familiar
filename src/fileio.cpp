#include "fileio.h"

#include "canvasscene.h"
#include "core/settingshandler.h"
#include "fml_archive.h"

#include <QBuffer>
#include <QEventLoop>
#include <QFile>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

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

bool is_image_large(const QImage& img)
{
    return img.width() > kLargeImageMaxDimension
           || img.height() > kLargeImageMaxDimension;
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

// Blocking download: load_images() runs on ThreadedIO's background
// thread, which has no event loop of its own to deliver
// QNetworkAccessManager's normally-async signals - a nested QEventLoop
// here blocks until this one reply completes, standard Qt idiom for
// "synchronous" network requests off the GUI thread.
//
// An unreachable/slow host would otherwise hang this loop (and the whole
// load_images() run, and its progress dialog) forever - a timeout, plus
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
        FLOG_DEBUG(Ch::IO,
                   "Download aborted (timed out or canceled): {}",
                   url.toString());
        reply->abort();
    } else if (reply->error() == QNetworkReply::NoError) {
        bytes = reply->readAll();
        img.loadFromData(bytes);
    } else {
        FLOG_DEBUG(Ch::IO, "Downloading image failed: {}", reply->errorString());
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

// ─── load_images ───────────────────────────────────────────────────────────

void load_images(const QList<QUrl>& urls,
                 const QPointF& pos,
                 CanvasScene* scene,
                 ThreadedIO* worker)
{
    QStringList errors;
    // Labels of images over kLargeImageMaxDimension, collected only in
    // "warn" mode (see below) and reported once via
    // ThreadedIO::largeImagesFound() after the loop.
    QStringList largeImages;
    const QString optimizeMode
        = SettingsHandler::getInstance()->autoOptimizeImportedImages();
    emit worker->beginProcessing(urls.size());

    // Only constructed if actually needed (most drops are local files).
    QNetworkAccessManager* netManager = nullptr;

    for (int i = 0; i < urls.size(); ++i) {
        const QUrl& rawUrl = urls.at(i);
        QImage img;
        QByteArray bytes;
        QString label;

        if (rawUrl.isLocalFile()) {
            label = rawUrl.toLocalFile();
            FLOG_DEBUG(Ch::IO, "Loading image from file {}", label);
            // Raw bytes read unconditionally (cheap - these are images,
            // not video), not just the decoded first frame: an animated
            // GIF needs the whole stream for GifItem's QMovie, and
            // is_animated() below needs something to sniff regardless.
            QFile file(label);
            if (file.open(QIODevice::ReadOnly)) {
                bytes = file.readAll();
            }
            QImageReader reader(label);
            reader.setAutoTransform(true); // apply EXIF rotation
            img = reader.read();
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
                FLOG_DEBUG(Ch::IO, "Downloading image from {}", label);
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
            FLOG_DEBUG(Ch::IO, "Could not load {}", label);
            errors.append(label);
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
        QPointF topLeft = pos - QPointF(img.width() / 2.0, img.height() / 2.0);

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
        scene->add_item_later(itemData, true);

        if (worker->canceled) {
            break;
        }
        worker->sleepMs(10);
    }

    delete netManager;

    if (!largeImages.isEmpty()) {
        emit worker->largeImagesFound(largeImages);
    }
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

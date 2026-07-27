#include "fileio.h"

#include "canvasscene.h"
#include "fml_archive.h"

#include <QEventLoop>
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
// "imgurl" query parameter. Unwrap it, the same way beeref's
// fileio/image.py's load_image() unwraps Pinterest page URLs.
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

// Decodes an embedded "data:[<mediatype>][;base64],<data>" URI - no
// network involved. Percent-decoding the payload before base64-decoding
// it is a no-op if the base64 alphabet survived QUrl's own encoding
// unchanged, and correctly undoes it if QUrl percent-encoded any of
// '+'/'/'/'=' along the way - safe either way.
QImage decode_data_url(const QUrl& url)
{
    QString full = url.toString(QUrl::FullyEncoded);
    if (!full.startsWith(QStringLiteral("data:"))) {
        return QImage();
    }
    QString payload = full.mid(5);
    int commaIdx = payload.indexOf(QLatin1Char(','));
    if (commaIdx < 0) {
        return QImage();
    }
    QString header = payload.left(commaIdx);
    QByteArray decoded = QByteArray::fromPercentEncoding(
        payload.mid(commaIdx + 1).toLatin1());

    QByteArray bytes = header.contains(QStringLiteral("base64"), Qt::CaseInsensitive)
        ? QByteArray::fromBase64(decoded)
        : decoded;

    QImage img;
    img.loadFromData(bytes);
    return img;
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
QImage download_image(QNetworkAccessManager& manager,
                      const QUrl& url,
                      ThreadedIO* worker)
{
    QNetworkRequest request(url);
    // Some CDNs/anti-bot setups throttle or stall the response body for
    // requests with no recognizable browser User-Agent (observed: fast
    // headers, then the transfer stalls) - claim to be an ordinary
    // browser, the same way e.g. PureRef's own downloader does.
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
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
    QObject::connect(&cancelPollTimer, &QTimer::timeout, &loop, [&loop, worker]() {
        if (worker->canceled) {
            loop.quit();
        }
    });
    cancelPollTimer.start(kCancelPollMs);

    loop.exec();

    QImage img;
    if (!reply->isFinished()) {
        FLOG_DEBUG(Ch::IO, "Download aborted (timed out or canceled): {}", url.toString());
        reply->abort();
    } else if (reply->error() == QNetworkReply::NoError) {
        img.loadFromData(reply->readAll());
    } else {
        FLOG_DEBUG(Ch::IO, "Downloading image failed: {}", reply->errorString());
    }
    reply->deleteLater();
    return img;
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
    emit worker->beginProcessing(urls.size());

    // Only constructed if actually needed (most drops are local files).
    QNetworkAccessManager* netManager = nullptr;

    for (int i = 0; i < urls.size(); ++i) {
        const QUrl& rawUrl = urls.at(i);
        QImage img;
        QString label;

        if (rawUrl.isLocalFile()) {
            label = rawUrl.toLocalFile();
            FLOG_DEBUG(Ch::IO, "Loading image from file {}", label);
            QImageReader reader(label);
            reader.setAutoTransform(true); // apply EXIF rotation
            img = reader.read();
        } else if (rawUrl.scheme().compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
            label = rawUrl.toString();
            FLOG_DEBUG(Ch::IO, "Decoding embedded data: image");
            img = decode_data_url(rawUrl);
        } else {
            QUrl url = unwrap_known_redirect(rawUrl);
            label = url.toString();
            if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
                || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0) {
                FLOG_DEBUG(Ch::IO, "Downloading image from {}", label);
                if (!netManager) {
                    netManager = new QNetworkAccessManager();
                }
                img = download_image(*netManager, url, worker);
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

        // Queue the raw data instead of constructing a PixmapItem here:
        // this runs on a background thread, and add_queued_items() (the
        // consumer) must only ever touch the QGraphicsScene from the GUI
        // thread. Center the item on `pos`, matching what
        // PixmapItem::set_pos_center() would do for a fresh item (scale 1,
        // rotation 0).
        QPointF topLeft = pos - QPointF(img.width() / 2.0, img.height() / 2.0);

        QVariantMap itemData;
        itemData[QStringLiteral("type")] = QStringLiteral("pixmap");
        itemData[QStringLiteral("image")] = img;
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
    FmlResult result
        = FmlArchive::save(scene, scene->rememberedBoundingRect(), filename, worker);

    if (worker) {
        emit worker->finished(result.error, result.itemErrors);
        worker->quit();
    }
    FLOG_DEBUG(Ch::IO, "End save");
}

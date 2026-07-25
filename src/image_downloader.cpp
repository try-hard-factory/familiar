#include "image_downloader.h"
#include "canvasscene.h"
#include "moveitem.h"

#include "log/log.h"
using namespace familiar::log;

ImageDownloader::ImageDownloader(CanvasScene& s, QObject* parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
    , scene_(s)
{}

ImageDownloader::~ImageDownloader()
{
    manager_->deleteLater();
}

void ImageDownloader::download(QString url, QPointF position)
{
    if (!isReady_)
        return;
    position_ = position;
    isReady_ = false;

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    reply_ = manager_->get(req);
    connect(reply_,
            SIGNAL(errorOccurred(QNetworkReply::NetworkError)),
            this,
            SLOT(errorOccurred(QNetworkReply::NetworkError)));

    connect(reply_, SIGNAL(finished()), this, SLOT(finished()));
}

void ImageDownloader::finished()
{
    QImage img;
    img.loadFromData(reply_->readAll());
    FLOG_DEBUG(Ch::Net, "{}", debugString(reply_->error()));
    FLOG_DEBUG(Ch::Net, "{}", reply_->url());
    FLOG_DEBUG(Ch::Net,
               "finished onDownloadFileComplete. {}",
               debugString(img.size()));
    isReady_ = true;
    reply_->deleteLater();
    // done
}

void ImageDownloader::errorOccurred(QNetworkReply::NetworkError err)
{
    FLOG_DEBUG(Ch::Net, "errorOccurred: {}", debugString(err));
    // Manage error here.
    reply_->deleteLater();
}

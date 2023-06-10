#include "image_downloader.h"
#include "canvasscene.h"
#include "moveitem.h"

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
    qDebug() << reply_->error();
    qDebug() << reply_->url();
    qDebug() << " finished onDownloadFileComplete. " << img.size();
    scene_.addImageToSceneToPosition(std::move(img), position_);
    isReady_ = true;
    reply_->deleteLater();
    // done
}

void ImageDownloader::errorOccurred(QNetworkReply::NetworkError err)
{
    qDebug() << "errorOccurred: " << err;
    // Manage error here.
    reply_->deleteLater();
}

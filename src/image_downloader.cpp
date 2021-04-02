#include "moveitem.h"
#include "image_downloader.h"

ImageDownloader::ImageDownloader(QGraphicsScene& s, QObject *parent)
    : QObject(parent), scene_(s)
{
    manager_ = new QNetworkAccessManager();
}


ImageDownloader::~ImageDownloader()
{
    manager_->deleteLater();
}


void ImageDownloader::setFile(QString url)
{
    if (!isReady_) return;
    isReady_ = false;

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    connect(manager_, SIGNAL(finished(QNetworkReply *)), this, SLOT(onDownloadFileComplete(QNetworkReply *)));

    manager_->get(req);
//    reply_ = manager_->get(req);
}


void ImageDownloader::onDownloadProgress(qint64 bytesRead, qint64 bytesTotal)
{
    qDebug(QString::number(bytesRead).toLatin1() +" - "+ QString::number(bytesTotal).toLatin1());
}

void ImageDownloader::onDownloadFileComplete(QNetworkReply *reply) {

    QImage img;
    img.loadFromData(reply->readAll());
    qDebug()<<"onDownloadFileComplete. "<<img.size();
    uint64_t z = 9999;
    MoveItem* item = new MoveItem(img, z);
    item->setPos({0, 0});

    scene_.addItem(item);
    isReady_ = true;

}

void ImageDownloader::onFinished(QNetworkReply * reply)
{
    switch(reply->error())
    {
        case QNetworkReply::NoError:
        {
            qDebug("file is downloaded successfully.");
            qDebug()<<reply->url().toString();
        }break;
        default:{
            qDebug(reply->errorString().toLatin1());
        };
    }
}


void ImageDownloader::onReadyRead()
{
    qDebug()<<"onReadyRead";
    QImage img;
    img.loadFromData(reply_->readAll());
    uint64_t z = 9999;
    MoveItem* item = new MoveItem(img, z);
    item->setPos({0, 0});

    scene_.addItem(item);
}


void ImageDownloader::onReplyFinished()
{
    qDebug()<<"onReplyFinished";
}

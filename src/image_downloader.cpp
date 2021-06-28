#include "moveitem.h"
#include "image_downloader.h"
#include "canvasscene.h"

ImageDownloader::ImageDownloader(CanvasScene& s, QObject *parent)
    : QObject(parent), scene_(s)
{
    manager_ = new QNetworkAccessManager();
}


ImageDownloader::~ImageDownloader()
{
    manager_->deleteLater();
}


void ImageDownloader::download(QString url, QPointF position)
{    
    if (!isReady_) return;
    position_ = position;
    isReady_ = false;

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    reply_ = manager_->get(req);
    connect(reply_, SIGNAL(errorOccurred(QNetworkReply::NetworkError)),
                this, SLOT(errorOccurred(QNetworkReply::NetworkError)));
//    connect(reply_, SIGNAL(downloadProgress(qint64, qint64)),
//                this, SLOT(updateProgress(qint64, qint64)));
    connect(reply_, SIGNAL(finished()),
                this, SLOT(finished()));

//    connect(manager_, SIGNAL(finished(QNetworkReply *)), this, SLOT(onDownloadFileComplete(QNetworkReply *)));
}

void ImageDownloader::finished()
{
    QImage img;
    img.loadFromData(reply_->readAll());
    qDebug()<<" finished onDownloadFileComplete. "<<img.size();
    scene_.addImageToSceneToPosition(std::move(img), position_);
    isReady_ = true;
    reply_->deleteLater();
    // done
}

void ImageDownloader::errorOccurred(QNetworkReply::NetworkError err)
{
    // Manage error here.
    reply_->deleteLater();
}



/*
 *UNUSED, delete it later
 */


void ImageDownloader::onDownloadProgress(qint64 bytesRead, qint64 bytesTotal)
{
    qDebug(QString::number(bytesRead).toLatin1() +" - "+ QString::number(bytesTotal).toLatin1());
}

void ImageDownloader::onDownloadFileComplete(QNetworkReply *reply) {

    QImage img;
    img.loadFromData(reply_->readAll());
    qDebug()<<"onDownloadFileComplete. "<<img.size();
    uint64_t z = 9999;
    QImage* img_ptr = new QImage(img);
    MoveItem* item = new MoveItem(img_ptr, z);
    item->setPos({0, 0});

    scene_.addItem(item);
    isReady_ = true;
    reply->deleteLater();
    reply_->deleteLater();
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
    QImage* img_ptr = new QImage(img);
    MoveItem* item = new MoveItem(img_ptr, z);
    item->setPos({0, 0});

    scene_.addItem(item);
}


void ImageDownloader::onReplyFinished()
{
    qDebug()<<"onReplyFinished";
}

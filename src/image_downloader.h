#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H

#include <QGraphicsScene>
#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QFile>
#include <QStringList>

class CanvasScene;

class ImageDownloader : public QObject
{
    Q_OBJECT
public:
    explicit ImageDownloader(CanvasScene& s, QObject *parent = nullptr);
    ~ImageDownloader();
    void download(QString url, QPointF position);
private:
    QNetworkAccessManager* manager_;
    QNetworkReply* reply_;
    CanvasScene& scene_;

private:
    bool isReady_ = true;
    QPointF position_ = {0,0};
private slots:
    void onDownloadProgress(qint64,qint64);
    void onFinished(QNetworkReply*);
    void onReadyRead();
    void onReplyFinished();
    void onDownloadFileComplete(QNetworkReply*);
    void errorOccurred(QNetworkReply::NetworkError err);
    void finished();
};

#endif // IMAGE_DOWNLOADER_H

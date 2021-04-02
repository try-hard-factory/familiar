#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H

#include <QGraphicsScene>
#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QFile>
#include <QStringList>

class ImageDownloader : public QObject
{
    Q_OBJECT
public:
    explicit ImageDownloader(QGraphicsScene& s, QObject *parent = nullptr);
    ~ImageDownloader();
    void setFile(QString url);
private:
    QNetworkAccessManager* manager_;
    QNetworkReply* reply_;
    QGraphicsScene& scene_;

private:
    bool isReady_ = true;

private slots:
    void onDownloadProgress(qint64,qint64);
    void onFinished(QNetworkReply*);
    void onReadyRead();
    void onReplyFinished();
    void onDownloadFileComplete(QNetworkReply*);
};

#endif // IMAGE_DOWNLOADER_H

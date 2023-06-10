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

/**
 * \~russian @brief ImageDownloader класс для скачивания картинок из браузера
 *
 * \~english @brief The ImageDownloader class for downloading images from the browser
 */
class ImageDownloader : public QObject
{
    Q_OBJECT
public:
    /**
     * \~russian @brief конструктор
     * \~russian @param s - ссылка на сцену
     * \~russian @param parent - указатель на QObject(может быть nullptr - это
     *                           нормально)
     *
     * \~english @brief Image downloader class constructor
     * \~english @param s - reference to scene
     * \~english @param parent - pointer to QObject parent(may be nullptr - it
     *                           is normal)
     */
    explicit ImageDownloader(CanvasScene& s, QObject *parent = nullptr);

    /**
     * \~russian @brief деструктор
     *
     * \~english @brief class destructor
     */
    ~ImageDownloader();

    /**
     * \~russian @brief скачать по указанному урлу картинку и добавить на сцену
     * \~russian @param url - адрес картинки
     * \~russian @param position - координаты на сцене
     *
     * \~english @brief download a picture at the specified url and add it to the scene
     * \~english @param url - image url-address
     * \~english @param position - coordinates on the scene
     */

    // \TODO: maybe rename this? or remove scene from this class and add callback
    void download(QString url, QPointF position);
private:
    QNetworkAccessManager* manager_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    CanvasScene& scene_;

private:
    bool isReady_ = true;
    QPointF position_ = {0,0};
private slots:
    void errorOccurred(QNetworkReply::NetworkError err);
    void finished();
};

#endif // IMAGE_DOWNLOADER_H

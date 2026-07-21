#include "fileio.h"

#include "canvasscene.h"

#include <QDebug>
#include <QImageReader>

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

void load_images(const QStringList& filenames,
                 const QPointF& pos,
                 CanvasScene* scene,
                 ThreadedIO* worker)
{
    QStringList errors;
    emit worker->beginProcessing(filenames.size());

    for (int i = 0; i < filenames.size(); ++i) {
        const QString& filename = filenames.at(i);
        qDebug() << "Loading image from file" << filename;

        QImageReader reader(filename);
        reader.setAutoTransform(true); // apply EXIF rotation
        QImage img = reader.read();
        emit worker->progress(i);

        if (img.isNull()) {
            qDebug() << "Could not load file" << filename;
            errors.append(filename);
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
        itemData[QStringLiteral("filename")] = filename;
        itemData[QStringLiteral("x")] = topLeft.x();
        itemData[QStringLiteral("y")] = topLeft.y();
        scene->add_item_later(itemData, true);

        if (worker->canceled) {
            break;
        }
        worker->sleepMs(10);
    }

    emit worker->finished(QString(), errors);
    worker->quit();
}

// ─── load_fml / save_fml ───────────────────────────────────────────────────

void load_fml(const QString& filename, CanvasScene* scene, ThreadedIO* worker)
{
    qDebug() << "Loading from file" << filename << "...";
    Q_UNUSED(scene)
    Q_UNUSED(worker)
    // TODOLATER: archive/manifest-based read, once that format is designed.
}

void save_fml(const QString& filename,
             CanvasScene* scene,
             bool createNew,
             ThreadedIO* worker)
{
    qDebug() << "Saving to file" << filename << "...";
    qDebug() << "Create new:" << createNew;
    Q_UNUSED(scene)
    Q_UNUSED(worker)
    // TODOLATER: archive/manifest-based write, once that format is designed.
    qDebug() << "End save";
}

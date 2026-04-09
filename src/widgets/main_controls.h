#pragma once

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QImage>
#include <QMimeData>
#include <QPoint>

// Forward declarations for control target interface
// Mixin - требуется чтобы Mixin имел методы:
// - scene() -> CanvasScene*
// - undo_stack() -> QUndoStack*
// - mapToScene(QPoint) -> QPointF
// - do_insert_images(QList<QUrl>, QPoint) -> void
// - open_from_file(QString) -> void

template<typename Mixin, typename T>
class MainControlsMixin : public T
{
public:
    explicit MainControlsMixin(T* parent = nullptr)
        : T(parent)
    {}

    void init_main_controls()
    {
        this->setContextMenuPolicy(Qt::CustomContextMenu);
        // TODOLATER: connect customContextMenuRequested to on_context_menu
        // connect(this, &QWidget::customContextMenuRequested,
        //         control_target, &Mixin::on_context_menu);
        this->setAcceptDrops(true);
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const auto* mimedata = event->mimeData();
        qDebug() << "Drag enter event:" << mimedata->formats();
        if (mimedata->hasUrls()) {
            event->acceptProposedAction();
        } else if (mimedata->hasImage()) {
            event->acceptProposedAction();
        } else {
            qDebug() << "Attempted drop not an image";
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override
    {
        const auto* mimedata = event->mimeData();
        qDebug() << "Handling file drop:" << mimedata->formats();

        Mixin* control_target = static_cast<Mixin*>(this);
        QPoint pos(qRound(event->position().x()), qRound(event->position().y()));

        if (mimedata->hasUrls()) {
            qDebug() << "Found dropped urls:" << mimedata->urls();

            // TODOLATER: Check if scene is empty and we have a bee/fml file to open directly
            // if (!control_target->scene()->items().isEmpty()) {
            //     QList<QUrl> urls = mimedata->urls();
            //     if (!urls.isEmpty() && urls[0].isLocalFile()) {
            //         QString path = urls[0].toLocalFile();
            //         if (is_bee_file(path)) {
            //             control_target->open_from_file(path);
            //             return;
            //         }
            //     }
            // }

            control_target->do_insert_images(mimedata->urls(), pos);
        } else if (mimedata->hasImage()) {
            // Handle direct image drop (e.g. from browser)
            QImage img = qvariant_cast<QImage>(mimedata->imageData());
            if (!img.isNull()) {
                // TODOLATER: create PixmapItem and insert via InsertItems command
                // auto* item = new PixmapItem();
                // item->setPixmap(QPixmap::fromImage(img));
                // QPointF scenePos = control_target->mapToScene(pos);
                // control_target->undo_stack()->push(
                //     new InsertItems(control_target->scene(), {item}, scenePos));
                qDebug() << "Image drop not yet implemented";
            }
        } else {
            qDebug() << "Drop not an image";
        }
    }
};

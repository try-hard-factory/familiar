#pragma once

#include "widgets/dialogs.h"
#include <core/controls.h>
#include <QCursor>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMimeData>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QWidget>
#include <qdebug.h>
template<typename Mixin, typename T>
class MainControlsMixin : public T
{
public:
    explicit MainControlsMixin(T* parent = nullptr)
        : T(parent)
    {}

    void init_main_controls(QWidget* mainWindow = nullptr)
    {
        mainWindow_ = mainWindow;
        this->setAcceptDrops(true);
    }

    bool mousePressEventMainControls(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton) {
            movewinStart_ = QCursor::pos();
            isMoving_= true;
            event->accept();
            return true;
        }

        return false;
    }

    bool mouseMoveEventMainControls(QMouseEvent* event)
    {
        if (event->buttons() & Qt::RightButton) {
            rightMoveFlag_ = true;
            if (isMoving_) {
                QPointF pos = static_cast<QWidget*>(this)->mapToGlobal(event->position());
                QPointF delta = pos - movewinStart_;
                movewinStart_ = pos;
                if (mainWindow_) {
                    mainWindow_->move(mainWindow_->x() + int(delta.x()),
                                    mainWindow_->y() + int(delta.y()));
                }
            }
            event->accept();
            return true;
        }

        return false;
    }

    bool mouseReleaseEventMainControls(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton) {
            if (!rightMoveFlag_) {
                static_cast<Mixin*>(this)->on_context_menu(event->position().toPoint());
            } else {
                rightMoveFlag_ = false;
            }
            isMoving_ = false;
            event->accept();
            return true;
        }

        return false;
    }

    bool keyPressEventMainControls(QKeyEvent* event)
    {
        exitMovewinMode();
        event->accept();
        return true;
    }

protected:
    QWidget* controlTarget_ = nullptr;

    void enterMovewinMode()
    {
        static_cast<QWidget*>(this)->setCursor(Qt::SizeAllCursor);
        movewinStart_ = QCursor::pos();
    }

    void exitMovewinMode()
    {
        static_cast<QWidget*>(this)->unsetCursor();
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const auto* mimedata = event->mimeData();
        qDebug() << "Drag enter event:" << mimedata->formats();
        if (mimedata->hasUrls()) {
            event->acceptProposedAction();
        } else if (mimedata->hasImage()) {
            event->acceptProposedAction();
        } else {
            const QString msg = "Attempted drop not an image or image too big";
            qDebug() << msg;
            FamNotification(controlTarget_, msg);
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

        Mixin* self = static_cast<Mixin*>(this);
        QPoint pos(qRound(event->position().x()), qRound(event->position().y()));

        if (mimedata->hasUrls()) {
            qDebug() << "Found dropped urls:" << mimedata->urls();
            self->do_insert_images(mimedata->urls(), pos);
        } else if (mimedata->hasImage()) {
            QImage img = qvariant_cast<QImage>(mimedata->imageData());
            if (!img.isNull()) {
                // TODOLATER: create PixmapItem and insert via InsertItems command
                qDebug() << "Image drop not yet implemented";
            }
        } else {
            qDebug() << "Drop not an image";
        }
    }

private:
    bool isMoving_ = false;
    bool rightMoveFlag_ = false;
    QPointF movewinStart_;
    QWidget* mainWindow_ = nullptr;
};

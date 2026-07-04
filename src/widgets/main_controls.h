#pragma once

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
#include <main_context_menu.h>
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
        this->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(
            static_cast<QWidget*>(this), &QWidget::customContextMenuRequested,
            static_cast<QObject*>(static_cast<T*>(this)),
            [this](const QPoint& pt) {
                QMetaObject::invokeMethod(
                    static_cast<QObject*>(static_cast<Mixin*>(this)),
                    "on_context_menu",
                    Qt::DirectConnection,
                    Q_ARG(QPoint, pt));
            });
        this->setAcceptDrops(true);
    }

    void on_action_movewin_mode()
    {
        // if (movewinActive_)
        //     exitMovewinMode();
        // else
        //     enterMovewinMode();
    }

    bool mousePressEventMainControls(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton) {
            qDebug() << "Right mouse button pressed !!!!!!";
            movewinStart_ = QCursor::pos();
            isMoving_= true;
            // enterMovewinMode();
            event->accept();
            return true;
        }

        // // if (movewinActive_) {
        //     exitMovewinMode();
        //     event->accept();
        //     return true;
        // // }
        // auto match = KeyboardSettings().mouseActionForEvent(event);
        // if (match && match->group == QLatin1String("movewindow")) {
        //     enterMovewinMode();
        //     event->accept();
        //     return true;
        // }
        // return false;
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
        // // if (movewinActive_) {
        //     QPointF pos = static_cast<QWidget*>(this)->mapToGlobal(event->position());
        //     QPointF delta = pos - movewinStart_;
        //     movewinStart_ = pos;
        //     if (mainWindow_)
        //         mainWindow_->move(mainWindow_->x() + int(delta.x()),
        //                           mainWindow_->y() + int(delta.y()));
        //     event->accept();
        //     return true;
        // // }
        // // return false;
    }

    bool mouseReleaseEventMainControls(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton) {
            if (!rightMoveFlag_) {
                qDebug() << "Right mouse button released CTX menu!!!!!!";
            } else {
                rightMoveFlag_ = false;
            }
            isMoving_ = false;
            event->accept();
            return true;
        }
        return false;
        // // if (movewinActive_) {
        //     exitMovewinMode();
        //     event->accept();
        //     return true;
        // // }
        // // return false;
    }

    bool keyPressEventMainControls(QKeyEvent* event)
    {
        // if (movewinActive_) {
            exitMovewinMode();
            event->accept();
            return true;
        // }
        // return false;
    }

protected:
    void enterMovewinMode()
    {
        // movewinActive_ = true;
        static_cast<QWidget*>(this)->setCursor(Qt::SizeAllCursor);
        movewinStart_ = QCursor::pos();
    }

    void exitMovewinMode()
    {
        // movewinActive_ = false;
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
    // bool movewinActive_ = false;
    bool isMoving_ = false;
    bool rightMoveFlag_ = false;
    QPointF movewinStart_;
    QWidget* mainWindow_ = nullptr;
};

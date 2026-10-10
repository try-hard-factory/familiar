#pragma once

#include "widgets/dialogs.h"
#include <core/controls.h>
#include <QCursor>
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

#include "log/log.h"
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

    bool mouse_press_event_main_controls(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton) {
            movewinStart_ = QCursor::pos();
            isMoving_ = true;
            event->accept();
            return true;
        }

        return false;
    }

    bool mouse_move_event_main_controls(QMouseEvent* event)
    {
        if (event->buttons() & Qt::RightButton) {
            rightMoveFlag_ = true;
            if (isMoving_) {
                const QPointF pos = static_cast<QWidget*>(this)->mapToGlobal(
                    event->position());
                const QPointF delta = pos - movewinStart_;
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

    bool mouse_release_event_main_controls(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton) {
            if (!rightMoveFlag_) {
                static_cast<Mixin*>(this)->on_context_menu(
                    event->position().toPoint());
            } else {
                rightMoveFlag_ = false;
            }
            isMoving_ = false;
            event->accept();
            return true;
        }

        return false;
    }

    bool key_press_event_main_controls(QKeyEvent* event)
    {
        if (isMoving_) {
            exit_movewin_mode();
            event->accept();
            return true;
        }
        return false;
    }

protected:
    // Set once by the mixing-in widget (CanvasView, WelcomeOverlay) -
    // which widget the notifications/move-window gestures act on.
    void set_control_target(QWidget* target) { controlTarget_ = target; }
    QWidget* control_target() const { return controlTarget_; }

    void enter_movewin_mode()
    {
        static_cast<QWidget*>(this)->setCursor(Qt::SizeAllCursor);
        movewinStart_ = QCursor::pos();
        isMoving_ = true;
    }

    void exit_movewin_mode()
    {
        isMoving_ = false;
        static_cast<QWidget*>(this)->unsetCursor();
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const auto* mimedata = event->mimeData();
        FLOG_DEBUG(familiar::log::Ch::UI,
                   "Drag enter event: {}",
                   familiar::log::debug_string(mimedata->formats()));
        if (mimedata->hasUrls()) {
            event->acceptProposedAction();
        } else if (mimedata->hasImage()) {
            event->acceptProposedAction();
        } else {
            const QString msg = "Attempted drop not an image or image too big";
            FLOG_DEBUG(familiar::log::Ch::UI, "{}", msg);
            FamNotification(control_target(), msg);
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        event->acceptProposedAction();
    }

    void dropEvent([[maybe_unused]] QDropEvent* event) override
    {
        FLOG_DEBUG(familiar::log::Ch::UI,
                   "MainControlMixin Handling file drop:");
    }

private:
    QWidget* controlTarget_ = nullptr;
    bool isMoving_ = false;
    bool rightMoveFlag_ = false;
    QPointF movewinStart_;
    QWidget* mainWindow_ = nullptr;
};

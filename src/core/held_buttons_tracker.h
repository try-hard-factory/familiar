#pragma once

#include <QObject>
#include <QPointer>
#include <QWidget>
#include <Qt>

// App-wide observer of which mouse buttons are currently held - installed
// as a QApplication event filter. Needed for mixed mouse+keyboard Action
// aliases (actions/action_mouse_dispatch.h): a keyPressEvent alone can't
// tell whether a mouse button is being held at the same time, since that
// state lives in separate mouse events.
//
// Also remembers which widget last received a MouseButtonPress, so a
// mixed alias firing mid-drag (e.g. holding the button that's driving a
// QGraphicsView rubber-band selection) can synthesize a matching release
// to that widget before invoking an action that might open a modal
// dialog - otherwise the press's widget never sees a release at all,
// leaving Qt's native drag/rubber-band state stuck (see
// ActionMouseDispatcher::tryKeyPress).
class HeldButtonsTracker : public QObject
{
    Q_OBJECT

public:
    static HeldButtonsTracker& instance();

    Qt::MouseButtons current() const { return held_; }
    QWidget* pressTarget() const { return pressTarget_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    HeldButtonsTracker() = default;

    Qt::MouseButtons held_ = Qt::NoButton;
    QPointer<QWidget> pressTarget_;
};

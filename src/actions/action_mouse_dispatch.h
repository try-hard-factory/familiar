#pragma once

#include <QObject>

class QWidget;
class QMouseEvent;
class QKeyEvent;

// App-wide dispatcher for Action mouse-chord and mixed mouse+key aliases
// (Action::get_mouse_bindings(), see actions/actions.h), plus bare-
// modifier-only keyboard shortcuts (Action::get_shortcuts() entries like
// "Ctrl" alone) - the counterpart to Qt's native QAction::setShortcuts()
// dispatch, which can't represent either of those. Installed once as a
// QApplication event filter (see MainWindow's constructor); invokeTarget
// is the widget whose slots Action::callback names resolve against,
// mirroring ActionsMixin::_create_actions()'s QMetaObject::invokeMethod
// convention.
class ActionMouseDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit ActionMouseDispatcher(QWidget* invokeTarget, QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool tryMousePress(QMouseEvent* event);
    bool tryKeyPress(QKeyEvent* event);
    bool tryBareModifierAction(QKeyEvent* event);

    QWidget* target_;
};

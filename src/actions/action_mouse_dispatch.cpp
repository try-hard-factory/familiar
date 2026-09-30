#include "action_mouse_dispatch.h"
#include "actions.h"

#include <canvasview.h>
#include <core/controls.h>
#include <core/held_buttons_tracker.h>
#include <log/log.h>
#include <mainwindow.h>
#include <tabpane.h>

#include <QApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextEdit>
#include <QWidget>

namespace {

QString button_name_for(Qt::MouseButton btn)
{
    for (const auto& pair : MouseConfigBase::button_map()) {
        if (pair.second == btn && pair.second != Qt::NoButton) {
            return pair.first;
        }
    }
    return {};
}

Qt::MouseButton button_flag_for(const QString& name)
{
    for (const auto& pair : MouseConfigBase::button_map()) {
        if (pair.first == name) {
            return pair.second;
        }
    }
    return Qt::NoButton;
}

void invoke(QWidget* target, const Action* action)
{
    if (action->callback.isEmpty()) {
        return;
    }
    QMetaObject::invokeMethod(target,
                              action->callback.toUtf8().constData(),
                              Qt::DirectConnection);
}

// A mixed alias fires while `flag` is physically held, meaning the
// current tab's canvas is very likely mid-interaction with it (e.g. a
// rubber-band selection drag - CanvasScene's own, not Qt's native
// QGraphicsView rubber band). It will never see a real release once the
// action's callback opens a modal dialog and steals input, leaving that
// drag stuck (CanvasScene::active_mode_ never resets, see
// canvasscene.cpp's mouseReleaseEvent). Synthesize the release directly
// to the canvas viewport - NOT HeldButtonsTracker::pressTarget(), which
// for this frameless/custom-mouse-handling MainWindow turned out to be
// MainWindow itself, not the canvas (confirmed via logging: the release
// was delivered but never reached CanvasView/CanvasScene at all).
void release_press_target_before_action(QWidget* invokeTarget,
                                    Qt::MouseButton flag,
                                    Qt::KeyboardModifiers modifiers)
{
    auto* mainWindow = qobject_cast<MainWindow*>(invokeTarget);
    CanvasView* canvasView = mainWindow ? mainWindow->tab_pane().current_widget()
                                        : nullptr;
    QWidget* releaseTarget = canvasView ? canvasView->viewport() : nullptr;
    if (!releaseTarget) {
        return;
    }

    const QPoint globalPos = QCursor::pos();
    QMouseEvent release(QEvent::MouseButtonRelease,
                        releaseTarget->mapFromGlobal(globalPos),
                        QPointF(globalPos),
                        flag,
                        Qt::NoButton,
                        modifiers);
    QApplication::sendEvent(releaseTarget, &release);
}

// Don't hijack normal typing (e.g. a filename field) with a bound
// shortcut just because it happens to match a key the user is typing.
bool focus_is_text_input()
{
    QWidget* focused = qApp->focusWidget();
    if (!focused) {
        return false;
    }
    return qobject_cast<QLineEdit*>(focused)
           || qobject_cast<QTextEdit*>(focused)
           || qobject_cast<QPlainTextEdit*>(focused);
}

const QSet<QString>& bare_modifier_names()
{
    static const QSet<QString> names = {QStringLiteral("Ctrl"),
                                        QStringLiteral("Shift"),
                                        QStringLiteral("Alt"),
                                        QStringLiteral("Meta")};
    return names;
}

} // namespace

ActionMouseDispatcher::ActionMouseDispatcher(QWidget* invokeTarget,
                                             QObject* parent)
    : QObject(parent)
    , target_(invokeTarget)
{}

bool ActionMouseDispatcher::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched)
    if (event->type() == QEvent::MouseButtonPress) {
        return try_mouse_press(static_cast<QMouseEvent*>(event));
    }
    if (event->type() == QEvent::KeyPress) {
        return try_key_press(static_cast<QKeyEvent*>(event));
    }
    return false;
}

bool ActionMouseDispatcher::try_mouse_press(QMouseEvent* event)
{
    const QString btn = button_name_for(event->button());
    if (btn.isEmpty()) {
        return false;
    }

    for (Action* action : get_actions().all()) {
        for (const Binding& b : action->get_mouse_bindings()) {
            if (!b.is_mouse_only() || b.mouseButton != btn) {
                continue;
            }
            if (MouseConfigBase::modifiers_to_qt(b.mouseModifiers)
                == event->modifiers()) {
                invoke(target_, action);
                return true;
            }
        }
    }
    return false;
}

bool ActionMouseDispatcher::try_key_press(QKeyEvent* event)
{
    // Bare-modifier Action shortcuts (just Ctrl/Alt/Shift/Meta alone, no
    // other key) - Qt's native QShortcutMap can't represent/match these
    // at all (QKeySequence has no "modifier alone" concept), so they're
    // dispatched here regardless of whether a mouse button is held. Tried
    // first since it doesn't depend on HeldButtonsTracker state.
    if (try_bare_modifier_action(event)) {
        return true;
    }

    const Qt::MouseButtons held = HeldButtonsTracker::instance().current();
    if (held == Qt::NoButton) {
        return false;
    }

    // Don't hijack normal typing (e.g. a filename field) just because the
    // user happens to be holding a mouse button incidentally.
    if (focus_is_text_input()) {
        return false;
    }

    const QString pressed = key_event_to_sequence_string(event);
    if (pressed.isEmpty()) {
        return false;
    }

    FLOG_DEBUG(familiar::log::Ch::UI,
               "tryKeyPress: held={} pressed='{}'",
               int(held),
               pressed.toStdString());

    for (Action* action : get_actions().all()) {
        for (const Binding& b : action->get_mouse_bindings()) {
            if (!b.is_mixed() || b.keySequence != pressed) {
                continue;
            }
            const Qt::MouseButton flag = button_flag_for(b.mouseButton);
            if (flag != Qt::NoButton && (held & flag)) {
                FLOG_DEBUG(familiar::log::Ch::UI,
                           "tryKeyPress: matched mixed alias for action '{}'",
                           action->id.toStdString());
                release_press_target_before_action(target_,
                                               flag,
                                               event->modifiers());
                invoke(target_, action);
                return true;
            }
        }
    }
    return false;
}

bool ActionMouseDispatcher::try_bare_modifier_action(QKeyEvent* event)
{
    const QString pressed = key_event_to_sequence_string(event);
    if (!bare_modifier_names().contains(pressed)) {
        return false;
    }
    if (focus_is_text_input()) {
        return false;
    }

    // Plain keyboard shortcuts (Action::get_shortcuts(), the same list
    // QAction::setShortcuts() reads) - not action->get_mouse_bindings(),
    // this has nothing to do with the mouse. Native QAction dispatch
    // already handles every other entry in that list; this only ever
    // fires for the bare-modifier ones it can't.
    for (Action* action : get_actions().all()) {
        if (action->get_shortcuts().contains(pressed)) {
            invoke(target_, action);
            return true;
        }
    }
    return false;
}

#pragma once

#include <QList>
#include <QWidget>

class BindingTarget;

// PureRef-style unified page: "Actions" (menu commands, actions/actions.h)
// and "Controls" (canvas mouse/wheel interactions, core/controls.h)
// collapsible sections, each a BindingsTreeWidget over the same alias
// model (widgets/controls/binding_target.h). Replaces the separate
// KeyboardShortcutsView/MouseView/MouseWheelView categories in
// ui/new_settings_window.cpp - those three widgets and their underlying
// single-binding-per-row tables still exist unchanged for the old
// ui/settings_window.cpp shell.
class KeyboardShortcutsPage : public QWidget
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsPage(QWidget* parent = nullptr);
    ~KeyboardShortcutsPage() override;

private:
    QList<BindingTarget*> actionTargets_;
    QList<BindingTarget*> controlTargets_;
};

#pragma once

#include <QList>
#include <QWidget>

class BindingTarget;
class BindingsTreeWidget;
class CollapsibleSection;

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

    // Filters both sections' rows down to targets whose name contains
    // `text`; hides a section entirely once it has no match left. Returns
    // whether the page has anything visible at all, so the sidebar
    // (ui/new_settings_window.cpp) can decide whether to show this
    // category when searching by content rather than by category name.
    bool applySearchFilter(const QString& text);

private:
    QList<BindingTarget*> actionTargets_;
    QList<BindingTarget*> controlTargets_;

    BindingsTreeWidget* actionsTree_ = nullptr;
    BindingsTreeWidget* controlsTree_ = nullptr;
    CollapsibleSection* actionsSection_ = nullptr;
    CollapsibleSection* controlsSection_ = nullptr;
};

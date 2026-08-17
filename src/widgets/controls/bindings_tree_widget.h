#pragma once

#include "binding_target.h"

#include <QMap>
#include <QWidget>

class QToolButton;

// Generic collapsible group header (chevron + title) wrapping any content
// widget - used for the "Actions"/"Controls" sections.
class CollapsibleSection : public QWidget
{
    Q_OBJECT

public:
    CollapsibleSection(const QString& title,
                       QWidget* content,
                       QWidget* parent = nullptr);

    void setExpanded(bool expanded);

private:
    QToolButton* headerBtn_ = nullptr;
    QWidget* content_ = nullptr;
};

// Flat list of rows, one (plus indented extra-alias rows) per
// BindingTarget: name + binding chip + "+"/"-" controls - see
// widgets/controls/binding_dialogs.h for the Add alias/Rebind dialogs
// this opens. Deliberately a plain QWidget/QVBoxLayout, not a
// QTreeWidget/QTableView: those manage their own scrolling viewport,
// which meant every section scrolled independently instead of the whole
// page sharing one scrollbar (matches the reference app: a single
// page-level scrollbar, sections are just expand/collapse within that
// one flow).
// Targets are not owned - the caller (KeyboardShortcutsPage) must
// outlive this widget.
class BindingsTreeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BindingsTreeWidget(const QList<BindingTarget*>& targets,
                                QWidget* parent = nullptr);

    // Rebuilds every row from current storage - e.g. after Restore
    // Defaults, which changes bindings out from under this widget without
    // going through its own Add/Remove/Rebind dialogs (those already call
    // refreshTarget() themselves).
    void refreshAll();

    // Hides rows whose target name doesn't contain `text` (case
    // insensitive) and bolds the matched substring in the ones that
    // remain; empty text shows everything unfiltered. Returns whether
    // any row is left visible, so the owner can hide/collapse an empty
    // section entirely.
    bool applySearchFilter(const QString& text);

signals:
    // A binding was added/removed/rebound (including a conflict getting
    // resolved by stealing a binding from some OTHER target, possibly in
    // a different BindingsTreeWidget entirely - e.g. an Action and a
    // Control conflicting over the same mouse chord). The owner
    // (KeyboardShortcutsPage) uses this to refresh every tree, since this
    // one only knows how to refresh its own rows.
    void bindingsChanged();

private:
    void refreshTarget(BindingTarget* target);
    QWidget* buildRow(BindingTarget* target,
                      const QString& label,
                      int bindingIndex,
                      bool showAdd,
                      bool indent,
                      QWidget* toggleTarget = nullptr);

    QList<BindingTarget*> targets_;
    QMap<BindingTarget*, QWidget*> rowContainers_;
    QString searchFilter_;
};

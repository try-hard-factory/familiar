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
// page sharing one scrollbar (PureRef reference: a single page-level
// scrollbar, sections are just expand/collapse within that one flow).
// Targets are not owned - the caller (KeyboardShortcutsPage) must
// outlive this widget.
class BindingsTreeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BindingsTreeWidget(const QList<BindingTarget*>& targets,
                                QWidget* parent = nullptr);

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
};

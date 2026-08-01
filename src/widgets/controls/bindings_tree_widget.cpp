#include "bindings_tree_widget.h"
#include "binding_dialogs.h"
#include "search_highlight.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

// ─── CollapsibleSection ───────────────────────────────────────────────────────

CollapsibleSection::CollapsibleSection(const QString& title,
                                       QWidget* content,
                                       QWidget* parent)
    : QWidget(parent)
    , content_(content)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    headerBtn_ = new QToolButton(this);
    headerBtn_->setText(title);
    headerBtn_->setCheckable(true);
    headerBtn_->setChecked(true);
    headerBtn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    headerBtn_->setArrowType(Qt::DownArrow);
    headerBtn_->setStyleSheet(
        QStringLiteral("QToolButton { border: none; font-weight: bold; }"));
    connect(headerBtn_, &QToolButton::toggled, this, [this](bool checked) {
        content_->setVisible(checked);
        headerBtn_->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    });

    layout->addWidget(headerBtn_);
    layout->addWidget(content_);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    headerBtn_->setChecked(expanded);
}

// ─── BindingsTreeWidget ───────────────────────────────────────────────────────

BindingsTreeWidget::BindingsTreeWidget(const QList<BindingTarget*>& targets,
                                       QWidget* parent)
    : QWidget(parent)
    , targets_(targets)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    for (BindingTarget* target : targets_) {
        auto* container = new QWidget(this);
        auto* containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);
        rowContainers_[target] = container;
        layout->addWidget(container);
        refreshTarget(target);
    }
}

void BindingsTreeWidget::refreshAll()
{
    for (BindingTarget* target : targets_)
        refreshTarget(target);
}

bool BindingsTreeWidget::applySearchFilter(const QString& text)
{
    searchFilter_ = text;
    bool anyVisible = false;
    for (BindingTarget* target : targets_) {
        QWidget* container = rowContainers_.value(target);
        if (!container)
            continue;
        const bool matches = text.isEmpty()
                             || target->text().contains(text,
                                                        Qt::CaseInsensitive);
        container->setVisible(matches);
        anyVisible = anyVisible || matches;
        // Rebuilds the row's label with the new searchFilter_ so the
        // matched substring gets bolded (or un-bolded, once cleared).
        refreshTarget(target);
    }
    return anyVisible;
}

void BindingsTreeWidget::refreshTarget(BindingTarget* target)
{
    QWidget* container = rowContainers_.value(target);
    if (!container)
        return;

    QLayout* rowLayout = container->layout();
    QLayoutItem* item;
    while ((item = rowLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QList<Binding> bindings = target->bindings();
    const bool hasAliases = bindings.size() > 1;

    // Extra aliases (index 1+) live in their own sub-container so the
    // primary row's chevron can show/hide all of them at once - matches
    // PureRef: a row only gets a chevron once it has more than one
    // binding.
    QWidget* extraContainer = nullptr;
    if (hasAliases) {
        extraContainer = new QWidget(container);
        auto* extraLayout = new QVBoxLayout(extraContainer);
        extraLayout->setContentsMargins(0, 0, 0, 0);
        extraLayout->setSpacing(0);
        for (int i = 1; i < bindings.size(); ++i) {
            extraLayout->addWidget(buildRow(target,
                                            QString(),
                                            i,
                                            /*showAdd=*/false,
                                            /*indent=*/true));
        }
    }

    rowLayout->addWidget(buildRow(target,
                                  target->text(),
                                  bindings.isEmpty() ? -1 : 0,
                                  /*showAdd=*/true,
                                  /*indent=*/false,
                                  extraContainer));
    if (extraContainer)
        rowLayout->addWidget(extraContainer);
}

QWidget* BindingsTreeWidget::buildRow(BindingTarget* target,
                                      const QString& label,
                                      int bindingIndex,
                                      bool showAdd,
                                      bool indent,
                                      QWidget* toggleTarget)
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(indent ? 24 : 4, 2, 4, 2);

    // Chevron only on a primary row that actually has aliases to
    // toggle - otherwise reserve the same width so labels still line up.
    if (toggleTarget) {
        auto* chevron = new QToolButton(row);
        chevron->setCheckable(true);
        chevron->setChecked(true);
        chevron->setArrowType(Qt::DownArrow);
        chevron->setAutoRaise(true);
        chevron->setStyleSheet(QStringLiteral("QToolButton { border: none; }"));
        connect(chevron,
                &QToolButton::toggled,
                this,
                [toggleTarget, chevron](bool checked) {
                    toggleTarget->setVisible(checked);
                    chevron->setArrowType(checked ? Qt::DownArrow
                                                  : Qt::RightArrow);
                });
        layout->addWidget(chevron);
    } else if (!indent) {
        auto* spacer = new QWidget(row);
        spacer->setFixedWidth(20);
        layout->addWidget(spacer);
    }

    auto* nameLabel = new QLabel(highlightSearchMatch(label, searchFilter_),
                                 row);
    layout->addWidget(nameLabel);
    layout->addStretch(1);

    if (bindingIndex >= 0) {
        const Binding b = target->bindings().value(bindingIndex);
        const QString chipLabel = b.displayText().isEmpty() ? tr("(none)")
                                                            : b.displayText();
        auto* chip = new QPushButton(chipLabel, row);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(QStringLiteral("QPushButton {"
                                           "  padding: 2px 8px;"
                                           "  border: 1px solid palette(mid);"
                                           "  border-radius: 4px;"
                                           "  background: palette(button);"
                                           "}"
                                           "QPushButton:hover {"
                                           "  background: palette(highlight);"
                                           "  color: palette(highlighted-text);"
                                           "}"));
        connect(chip,
                &QPushButton::clicked,
                this,
                [this, target, bindingIndex]() {
                    auto* dlg = new RebindDialog(target, bindingIndex, this);
                    connect(dlg, &QDialog::accepted, this, [this, target]() {
                        refreshTarget(target);
                        emit bindingsChanged();
                    });
                    dlg->exec();
                    dlg->deleteLater();
                });
        layout->addWidget(chip);

        // No "-" on the primary slot (index 0) - that's the default
        // binding, not an alias, whether or not its current value still
        // matches the shipped default (Rebind, including its own clear
        // "×" buttons, is how you change/clear it). Only genuine aliases
        // (index > 0) are removable this way.
        if (bindingIndex > 0) {
            auto* removeBtn = new QToolButton(row);
            removeBtn->setText(QStringLiteral("-"));
            connect(removeBtn,
                    &QToolButton::clicked,
                    this,
                    [this, target, bindingIndex]() {
                        QList<Binding> bindings = target->bindings();
                        if (bindingIndex >= 0
                            && bindingIndex < bindings.size()) {
                            bindings.removeAt(bindingIndex);
                            target->setBindings(bindings);
                        }
                        refreshTarget(target);
                        emit bindingsChanged();
                    });
            layout->addWidget(removeBtn);
        }
    }

    if (showAdd) {
        auto* addBtn = new QToolButton(row);
        addBtn->setText(QStringLiteral("+"));
        connect(addBtn, &QToolButton::clicked, this, [this, target]() {
            auto* dlg = new AddAliasDialog(target, this);
            connect(dlg, &QDialog::accepted, this, [this, target]() {
                refreshTarget(target);
                emit bindingsChanged();
            });
            dlg->exec();
            dlg->deleteLater();
        });
        layout->addWidget(addBtn);
    }

    return row;
}

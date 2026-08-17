#include "bindings_tree_widget.h"
#include "binding_dialogs.h"
#include "search_highlight.h"

#include <widgets/setting_descriptions.h>
#include <widgets/setting_row.h>
#include <widgets/settings_style.h>

#include <QHBoxLayout>
#include <QLayoutItem>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace {
// Every shortcut chip gets at least this width, so the column of badges
// lines up instead of each one shrink-wrapping its own text (QPushButton
// centers its text by default, so short labels like "Ctrl+P" just end up
// centered in the extra space). It's a floor, not a cap - setMinimumWidth
// rather than setFixedWidth - so a longer chord (mouse bindings can read
// e.g. "Left MB + Ctrl+Alt+Shift") still grows to fit instead of clipping.
constexpr int kChipMinWidth = 104;

// A target's primary row (buildRow()'s showAdd==true case) - adds the
// same Ctrl+double-click-to-reset gesture SettingRowBase's rows have
// (widgets/setting_row.h/.cpp), reset here meaning "replace this
// target's whole binding list (primary + every alias) with
// defaultBindings()". Not applied to alias sub-rows: an individual alias
// has no "default" of its own to reset to, only the target as a whole
// does.
class BindingRowWidget : public QWidget
{
public:
    using ResetCallback = std::function<void()>;

    BindingRowWidget(ResetCallback onReset, QWidget* parent)
        : QWidget(parent)
        , onReset_(std::move(onReset))
    {}

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            event->accept();
            // Deferred: onReset_ ends up calling
            // BindingsTreeWidget::refreshTarget(), which immediately
            // deletes this very row widget (same rebuild-in-place
            // pattern the existing remove/add-alias buttons already
            // trigger from their own clicked() handlers) - unlike those
            // (a child QToolButton's slot deleting its parent), this
            // would be deleting `this` from inside `this` own in-progress
            // virtual event handler, still being unwound by
            // QWidget::event()/QApplication::notify() above us on the
            // call stack. QTimer::singleShot(0, ...) runs it on a fresh
            // stack frame right after, once nothing above us still
            // expects `this` to be alive.
            QTimer::singleShot(0, this, [callback = onReset_]() { callback(); });
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    ResetCallback onReset_;
};
} // namespace

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
        QStringLiteral(
            "QToolButton { border: none; font-weight: bold; color: %1; }")
            .arg(familiar::settings_style::palette().text.name()));
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
    for (BindingTarget* target : targets_) {
        refreshTarget(target);
    }
}

bool BindingsTreeWidget::applySearchFilter(const QString& text)
{
    searchFilter_ = text;
    bool anyVisible = false;
    for (BindingTarget* target : targets_) {
        QWidget* container = rowContainers_.value(target);
        if (!container) {
            continue;
        }
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
    if (!container) {
        return;
    }

    QLayout* rowLayout = container->layout();
    QLayoutItem* item;
    while ((item = rowLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QList<Binding> bindings = target->bindings();
    const bool hasAliases = bindings.size() > 1;

    // Extra aliases (index 1+) live in their own sub-container so the
    // primary row's chevron can show/hide all of them at once - a row
    // only gets a chevron once it has more than one binding.
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
    if (extraContainer) {
        rowLayout->addWidget(extraContainer);
    }
}

QWidget* BindingsTreeWidget::buildRow(BindingTarget* target,
                                      const QString& label,
                                      int bindingIndex,
                                      bool showAdd,
                                      bool indent,
                                      QWidget* toggleTarget)
{
    // showAdd is only true for a target's primary row (see refreshTarget()'s
    // two buildRow() call sites) - BindingRowWidget's Ctrl+double-click
    // reset gesture only makes sense there (see its own comment above).
    QWidget* row = showAdd
                       ? new BindingRowWidget(
                             [this, target]() {
                                 target->setBindings(target->defaultBindings());
                                 refreshTarget(target);
                                 emit bindingsChanged();
                             },
                             this)
                       : new QWidget(this);
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
        chevron->setStyleSheet(
            QStringLiteral("QToolButton { border: none; color: %1; }")
                .arg(familiar::settings_style::palette().text.name()));
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

    // HoverInfoLabel (widgets/setting_row.h) - same hover-tooltip label
    // the Performance/Images & Items settings rows use, for a consistent
    // "hover for more info" affordance across the whole Settings window.
    // Body text comes from widgets/setting_descriptions.h, keyed by
    // target->id() - same single-source-of-truth table those pages'
    // own rows read from (currently still a shared placeholder for
    // every id here, real per-action/control copy is a follow-up).
    auto* nameLabel
        = new HoverInfoLabel(highlightSearchMatch(label, searchFilter_), row);
    nameLabel->setInfoText(
        familiar::setting_descriptions::forBindingTargetId(target->id()));
    // "Default: X" / the reset-gesture footer hint only apply to the
    // primary row - matches BindingRowWidget's own showAdd-only gate
    // above, and for the same reason (an alias has no "default" of its
    // own).
    if (showAdd) {
        QStringList defaultChords;
        for (const Binding& b : target->defaultBindings()) {
            const QString dt = b.displayText();
            if (!dt.isEmpty()) {
                defaultChords.append(dt);
            }
        }
        nameLabel->setDefaultText(
            tr("Default: %1")
                .arg(defaultChords.isEmpty()
                         ? tr("(none)")
                         : defaultChords.join(QStringLiteral(", "))));
        nameLabel->setShowResetHint(true);
    }
    layout->addWidget(nameLabel);
    layout->addStretch(1);

    if (bindingIndex >= 0) {
        const Binding b = target->bindings().value(bindingIndex);
        const QString chipLabel = b.displayText().isEmpty() ? tr("(none)")
                                                            : b.displayText();
        auto* chip = new QPushButton(chipLabel, row);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setMinimumWidth(kChipMinWidth);
        chip->setStyleSheet(familiar::settings_style::shortcutChipStyleSheet());
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
            removeBtn->setCursor(Qt::PointingHandCursor);
            removeBtn->setStyleSheet(
                familiar::settings_style::miniButtonStyleSheet());
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
        addBtn->setCursor(Qt::PointingHandCursor);
        addBtn->setStyleSheet(familiar::settings_style::miniButtonStyleSheet());
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

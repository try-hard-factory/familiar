#include "keyboard_shortcuts_page.h"
#include "binding_target.h"
#include "bindings_tree_widget.h"

#include <actions/actions.h>
#include <core/controls.h>
#include <core/settings.h>

#include <QFrame>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

KeyboardShortcutsPage::KeyboardShortcutsPage(QWidget* parent)
    : QWidget(parent)
{
    static const QSet<QString> shownUnderControls = {
        QStringLiteral("zoom_in"),
        QStringLiteral("zoom_out"),
    };

    for (Action* action : getActions().all()) {
        if (action->id.startsWith(QLatin1String("recent_files_"))) {
            continue;
        }
        if (action->text.isEmpty()) {
            continue;
        }
        if (shownUnderControls.contains(action->id)) {
            continue;
        }
        actionTargets_.append(new ActionBindingTarget(action));
    }

    for (const MouseConfig& cfg : KeyboardSettings::mouseActions()) {
        controlTargets_.append(
            new MouseConfigBindingTarget(&cfg, BindingTargetKind::MouseControl));
        if (cfg.id() == QLatin1String("zoom")) {
            if (Action* zoomIn = getActions().find(QStringLiteral("zoom_in"))) {
                controlTargets_.append(new ActionBindingTarget(zoomIn));
            }
            if (Action* zoomOut = getActions().find(
                    QStringLiteral("zoom_out"))) {
                controlTargets_.append(new ActionBindingTarget(zoomOut));
            }
        }
    }
    for (const MouseWheelConfig& cfg : KeyboardSettings::mousewheelActions()) {
        controlTargets_.append(
            new MouseConfigBindingTarget(&cfg,
                                         BindingTargetKind::MouseWheelControl));
    }

    actionsTree_ = new BindingsTreeWidget(actionTargets_, this);
    controlsTree_ = new BindingsTreeWidget(controlTargets_, this);

    // Restore Defaults wipes storage out from under these targets without
    // going through any of the tree's own Add/Remove/Rebind dialogs (which
    // already refresh themselves) - rebuild every row when it fires.
    connect(&SettingsEvents::instance(),
            &SettingsEvents::restoreKeyboardDefaults,
            actionsTree_,
            &BindingsTreeWidget::refreshAll);
    connect(&SettingsEvents::instance(),
            &SettingsEvents::restoreKeyboardDefaults,
            controlsTree_,
            &BindingsTreeWidget::refreshAll);

    // A conflict can be resolved by stealing a binding from a target in
    // the OTHER tree (an Action and a Control can now conflict over the
    // same mouse chord) - each tree only knows how to refresh its own
    // rows, so wire both trees' changes to refresh both.
    connect(actionsTree_,
            &BindingsTreeWidget::bindingsChanged,
            actionsTree_,
            &BindingsTreeWidget::refreshAll);
    connect(actionsTree_,
            &BindingsTreeWidget::bindingsChanged,
            controlsTree_,
            &BindingsTreeWidget::refreshAll);
    connect(controlsTree_,
            &BindingsTreeWidget::bindingsChanged,
            controlsTree_,
            &BindingsTreeWidget::refreshAll);
    connect(controlsTree_,
            &BindingsTreeWidget::bindingsChanged,
            actionsTree_,
            &BindingsTreeWidget::refreshAll);

    auto* scrollContent = new QWidget(this);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    actionsSection_ = new CollapsibleSection(tr("Actions"),
                                             actionsTree_,
                                             scrollContent);
    controlsSection_ = new CollapsibleSection(tr("Controls"),
                                              controlsTree_,
                                              scrollContent);
    scrollLayout->addWidget(actionsSection_);
    scrollLayout->addWidget(controlsSection_);
    scrollLayout->addStretch(1);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(scrollContent);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scrollArea);
}

KeyboardShortcutsPage::~KeyboardShortcutsPage()
{
    qDeleteAll(actionTargets_);
    qDeleteAll(controlTargets_);
}

bool KeyboardShortcutsPage::applySearchFilter(const QString& text)
{
    const bool actionsMatch = actionsTree_->applySearchFilter(text);
    const bool controlsMatch = controlsTree_->applySearchFilter(text);
    actionsSection_->setVisible(actionsMatch);
    controlsSection_->setVisible(controlsMatch);
    // Auto-expand a matching section while searching, so results aren't
    // hidden behind a chevron the user collapsed earlier.
    if (!text.isEmpty()) {
        if (actionsMatch) {
            actionsSection_->setExpanded(true);
        }
        if (controlsMatch) {
            controlsSection_->setExpanded(true);
        }
    }
    return actionsMatch || controlsMatch;
}

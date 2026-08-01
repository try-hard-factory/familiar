#include "keyboard_shortcuts_page.h"
#include "binding_target.h"
#include "bindings_tree_widget.h"

#include <actions/actions.h>
#include <core/controls.h>

#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

KeyboardShortcutsPage::KeyboardShortcutsPage(QWidget* parent)
    : QWidget(parent)
{
    // Keyboard-only, but conceptually belong with the other zoom/pan
    // Controls rather than buried in the flat Actions list - shown there
    // instead, per Max's request.
    static const QSet<QString> shownUnderControls = {
        QStringLiteral("zoom_in"),
        QStringLiteral("zoom_out"),
    };

    for (Action* action : getActions().all()) {
        if (action->id.startsWith(QLatin1String("recent_files_")))
            continue;
        if (action->text.isEmpty())
            continue;
        if (shownUnderControls.contains(action->id))
            continue;
        actionTargets_.append(new ActionBindingTarget(action));
    }

    for (const MouseConfig& cfg : KeyboardSettings::mouseActions()) {
        controlTargets_.append(
            new MouseConfigBindingTarget(&cfg, BindingTargetKind::MouseControl));
        if (cfg.id() == QLatin1String("zoom")) {
            if (Action* zoomIn = getActions().find(QStringLiteral("zoom_in")))
                controlTargets_.append(new ActionBindingTarget(zoomIn));
            if (Action* zoomOut = getActions().find(QStringLiteral("zoom_out")))
                controlTargets_.append(new ActionBindingTarget(zoomOut));
        }
    }
    for (const MouseWheelConfig& cfg : KeyboardSettings::mousewheelActions()) {
        controlTargets_.append(
            new MouseConfigBindingTarget(&cfg,
                                         BindingTargetKind::MouseWheelControl));
    }

    auto* actionsTree = new BindingsTreeWidget(actionTargets_, this);
    auto* controlsTree = new BindingsTreeWidget(controlTargets_, this);

    auto* scrollContent = new QWidget(this);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->addWidget(
        new CollapsibleSection(tr("Actions"), actionsTree, scrollContent));
    scrollLayout->addWidget(
        new CollapsibleSection(tr("Controls"), controlsTree, scrollContent));
    scrollLayout->addStretch(1);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
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

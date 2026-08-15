#pragma once

#include <QString>

// Single home for every settings-row/binding-row hover description
// (widgets/setting_row.h's SettingInfoPopup body text) - one place to
// read/edit all of them, instead of hunting through each row class's own
// constructor call site scattered across setting_row.cpp and
// bindings_tree_widget.cpp. Anything not written up yet falls back to a
// shared placeholder rather than needing its own explicit entry.
namespace familiar::setting_descriptions {

// Looked up by SettingRowBase's constructor (widgets/setting_row.cpp),
// keyed by the FamSettings storage key it was constructed with (e.g.
// "Items/arrange_gap", core/settings.cpp's fields() table).
QString forSettingsKey(const QString& key);

// Looked up by BindingsTreeWidget::buildRow() (widgets/controls/
// bindings_tree_widget.cpp), keyed by BindingTarget::id() - an
// Action::id for a keyboard shortcut row, or a Mouse/MouseWheel config
// id (core/controls.h) for a Controls row.
QString forBindingTargetId(const QString& id);

} // namespace familiar::setting_descriptions

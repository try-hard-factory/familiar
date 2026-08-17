#include "setting_descriptions.h"

#include <QMap>

namespace familiar::setting_descriptions {

namespace {

// TODOLATER: Performance page rows (Undo History Size, Auto Optimize
// Imported Images, Autosave) and every Keyboard Shortcuts row still fall
// back to this - real copy for those is a follow-up.
const QString& placeholder()
{
    static const QString text = QStringLiteral("Description coming soon.");
    return text;
}

// Rich HTML is fine here - SettingInfoPopup's body QLabel
// (widgets/setting_row.cpp) is explicitly Qt::RichText, so <b>, <br/>,
// <ul>/<li>, etc. all render (same subset QLabel/QTextDocument always
// support elsewhere in this app, e.g. HelpDialog). Keep it to inline
// tags, not full <html>/<body> wrapping - QLabel doesn't need it.
const QMap<QString, QString>& settingsTable()
{
    static const QMap<QString, QString> table = {
        {QStringLiteral("Items/arrange_gap"),
         QStringLiteral("The gap between images when using arrange actions.")},
        {QStringLiteral("Items/image_allocation_limit"),
         QStringLiteral(
             "The maximum image size that can be loaded (in megabytes)."
             "<br/><br/><b>Set to 0</b> for no limitation.")},
        {QStringLiteral("Items/arrange_default"),
         QStringLiteral(
             "How images are arranged when inserted in batch."
             "<br/><br/><b>Optimal</b> packs images to minimize wasted "
             "space; the others sort <b>by filename</b> along one axis.")},
    };
    return table;
}

// Keyed by BindingTarget::id() - for an Action this is Action::id
// (actions/actions.cpp's A::make() calls), not its display text, e.g.
// "open" for the Ctrl+O row, not "Open".
const QMap<QString, QString>& bindingTargetsTable()
{
    static const QMap<QString, QString> table = {
        {QStringLiteral("open"),
         QStringLiteral(
             "Opens an existing scene file."
             "<br/><br/>If the current scene has unsaved changes, you'll "
             "be asked what to do with them first.")},
    };
    return table;
}

} // namespace

QString forSettingsKey(const QString& key)
{
    return settingsTable().value(key, placeholder());
}

QString forBindingTargetId(const QString& id)
{
    return bindingTargetsTable().value(id, placeholder());
}

} // namespace familiar::setting_descriptions

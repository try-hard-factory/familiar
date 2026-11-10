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
const QMap<QString, QString>& settings_table()
{
    static const QMap<QString, QString> table = {
        {QStringLiteral("Telemetry/enabled"),
         QStringLiteral(
             "Sends two things to familiar's own server, at most once a "
             "day: a random number generated on this computer, and the "
             "version you are running."
             "<br/><br/>Nothing else - no file names, no images, no canvas "
             "contents, no name or email. The server stores no IP "
             "addresses and no time of day, only the date."
             "<br/><br/>It exists to count installations and launches, and "
             "to see which versions are still in use."
             "<br/><br/>Setting <b>DO_NOT_TRACK=1</b> in the environment, "
             "or passing <b>--no-telemetry</b>, overrides this for that "
             "run.")},
        {QStringLiteral("Items/arrange_gap"),
         QStringLiteral("The gap between images when using arrange actions.")},
        {QStringLiteral("Items/image_allocation_limit"),
         QStringLiteral(
             "The maximum image size that can be loaded (in megabytes)."
             "<br/><br/><b>Set to 0</b> for no limitation.")},
        {QStringLiteral("Items/raw_import_choice"),
         QStringLiteral(
             "How camera RAW files (.NEF, .CR3, ...) are converted when "
             "imported."
             "<br/><br/><b>Optimize</b> converts faster at slightly lower "
             "quality; <b>Keep original</b> uses the best conversion your "
             "camera's profile allows, which can take several seconds per "
             "file."
             "<br/><br/>Set this back to <b>Ask every time</b> to bring the "
             "import prompt back after ticking \"Remember choice for future "
             "files\".")},
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
const QMap<QString, QString>& binding_targets_table()
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

QString for_settings_key(const QString& key)
{
    return settings_table().value(key, placeholder());
}

QString for_binding_target_id(const QString& id)
{
    return binding_targets_table().value(id, placeholder());
}

} // namespace familiar::setting_descriptions

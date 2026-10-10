#include "settingshandler.h"
#include <cmath>
#include <functional>
#include <limits>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSharedPointer>
#include <QStandardPaths>

#include <core/settings.h>
#include <core/valuehandler.h>

#include "log/log.h"
#include "utils/utils.h"
using namespace familiar::log;

#define OPTION(KEY, TYPE) \
    {QStringLiteral(KEY), QSharedPointer<ValueHandler>(new TYPE)}

namespace {

QMap<int, int> opacityListDef = {
    {kDarkPreset, 255},
    {kLightPreset, 255},
    {kCustom1, 255},
    {kCustom2, 255},
    {kCustom3, 255},
    {kCustom4, 255},
};

QMap<int, QColor> darkColorPresetDef
    = {{kBackgroundColor, QColor({32, 32, 32})},   // kBackgroundColor
       {kCanvasColor, QColor({42, 42, 42})},       // kCanvasColor
       {kBorderColor, QColor({13, 13, 13})},       // kBorderColor
       {kTextColor, QColor({255, 255, 255})},      // kTextColor - white
       {kSelectionColor, QColor({22, 142, 153})}}; // kSelectionColor
QMap<int, QColor> lightColorPresetDef
    = {{kBackgroundColor, QColor({224, 224, 224})},
       {kCanvasColor, QColor({234, 234, 234})},
       {kBorderColor, QColor({200, 200, 200})},
       {kTextColor, QColor({111, 111, 111})},
       {kSelectionColor, QColor({255, 0, 0})}};
QMap<int, QColor> customPreset1Def = {{kBackgroundColor, QColor({32, 32, 32})},
                                      {kCanvasColor, QColor({42, 42, 42})},
                                      {kBorderColor, QColor({13, 13, 13})},
                                      {kTextColor, QColor({122, 122, 122})},
                                      {kSelectionColor, QColor({22, 142, 153})}};
QMap<int, QColor> customPreset2Def = {{kBackgroundColor, QColor({32, 32, 32})},
                                      {kCanvasColor, QColor({42, 42, 42})},
                                      {kBorderColor, QColor({13, 13, 13})},
                                      {kTextColor, QColor({122, 122, 122})},
                                      {kSelectionColor, QColor({22, 142, 153})}};
QMap<int, QColor> customPreset3Def = {{kBackgroundColor, QColor({32, 32, 32})},
                                      {kCanvasColor, QColor({42, 42, 42})},
                                      {kBorderColor, QColor({13, 13, 13})},
                                      {kTextColor, QColor({122, 122, 122})},
                                      {kSelectionColor, QColor({22, 142, 153})}};
QMap<int, QColor> customPreset4Def = {{kBackgroundColor, QColor({32, 32, 32})},
                                      {kCanvasColor, QColor({42, 42, 42})},
                                      {kBorderColor, QColor({13, 13, 13})},
                                      {kTextColor, QColor({122, 122, 122})},
                                      {kSelectionColor, QColor({22, 142, 153})}};

QMap<class QString, QSharedPointer<ValueHandler>> recognizedGeneralOptions = {
    //         KEY                            TYPE                 DEFAULT_VALUE
    OPTION("option0", Bool(true)),
    OPTION("option1", Bool(true)),
    OPTION("current_preset",
           BoundedInt(0, EPresets::kAllPresets, EPresets::kDarkPreset)),
    OPTION("master_opacity", OpacityList(opacityListDef)),
    OPTION("dark_color_preset", ColorList(darkColorPresetDef)),
    OPTION("light_color_preset", ColorList(lightColorPresetDef)),
    OPTION("custom_preset1", ColorList(customPreset1Def)),
    OPTION("custom_preset2", ColorList(customPreset2Def)),
    OPTION("custom_preset3", ColorList(customPreset3Def)),
    OPTION("custom_preset4", ColorList(customPreset4Def)),

};

// QJsonValue::toVariant() doesn't guarantee int over double for whole
// numbers, but ValueHandler::check() implementations (e.g. BoundedInt)
// go through QVariant::toString().toInt() - safest to pin whole numbers
// down to a real int right at the JSON/QVariant boundary, once, here.
QVariant json_to_variant(const QJsonValue& v)
{
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (d == std::trunc(d)
            && std::abs(d)
                   <= static_cast<double>(std::numeric_limits<int>::max())) {
            return static_cast<int>(d);
        }
        return d;
    }
    return v.toVariant();
}

// ── Schema versioning ──────────────────────────────────────────────────
// Mirrors the .fml project format's formatVersion/migration design (see
// docs/fml_format_design.md §6), applied to settings.json instead - kept
// deliberately separate from it (a settings-file quirk shouldn't block
// opening a project, and vice versa).
constexpr int ksettingsSchemaVersion = 1;
constexpr char kschemaVersionKey[] = "schemaVersion";

// {fromVersion: transform} - migrations[N] upgrades a document from
// schema N to N+1; applied in ascending order until the document reaches
// kSettingsSchemaVersion. Empty for now - no breaking change has shipped
// yet (the app itself is unreleased, so there's nothing real to migrate
// from) - but the mechanism exists so the first real migration has
// somewhere to go instead of being invented from scratch under time
// pressure. Only bump kSettingsSchemaVersion for a change that actually
// breaks reading old data (renamed/restructured key) - purely additive
// changes (a new key with its own default, like every FamSettings field
// added so far) don't need one.
const QMap<int, std::function<void(QJsonObject&)>>& settings_migrations()
{
    static const QMap<int, std::function<void(QJsonObject&)>> migrations = {
        // {1, [](QJsonObject& doc) { ... }},
    };
    return migrations;
}

// Unlike .fml's parse_manifest() (which refuses to load a file newer
// than the app supports - project data is worth protecting from a
// half-understood load), a settings.json from a newer familiar is loaded
// best-effort: every FamSettings field already falls back to its own
// default on a bad/unrecognized value (see FamSettings::valueOrDefault()),
// so the worst case is a handful of settings resetting to default, not a
// hard failure to start the app over a non-critical file.
void apply_settings_migrations(QJsonObject& doc)
{
    int version = doc.value(QLatin1String(kschemaVersionKey))
                      .toInt(ksettingsSchemaVersion);
    if (version > ksettingsSchemaVersion) {
        FLOG_WARN(Ch::Settings,
                  "settings.json has schemaVersion {} - newer than this "
                  "build supports ({}) - loading best-effort",
                  version,
                  ksettingsSchemaVersion);
        return;
    }
    const auto& migrations = settings_migrations();
    while (version < ksettingsSchemaVersion) {
        auto it = migrations.find(version);
        if (it != migrations.end()) {
            it.value()(doc);
        }
        ++version;
    }
    doc[QLatin1String(kschemaVersionKey)] = ksettingsSchemaVersion;
}

} // namespace

SettingsHandler::SettingsHandler()
{
    settingsFilePath_ = CommandlineArgs::instance().settings_file();
    if (settingsFilePath_.isEmpty()) {
        QString dir = portable_data_dir();
        if (dir.isEmpty()) {
            dir = QStandardPaths::writableLocation(
                QStandardPaths::AppConfigLocation);
        }
        settingsFilePath_ = QDir(dir).filePath(QStringLiteral("settings.json"));
    }
    load_document();
}


SettingsHandler* SettingsHandler::get_instance()
{
    static SettingsHandler config;
    return &config;
}

void SettingsHandler::load_document()
{
    QFile file(settingsFilePath_);
    QJsonObject doc;
    // Not open() failing on its own - that's the normal, expected first
    // run (no settings.json yet), nothing worth warning about. Only a
    // file that DID open but didn't parse as a JSON object is a real
    // problem (corrupt settings.json) - silently falls back to an empty
    // document/defaults either way (deliberate, see this function's own
    // doc comment), but that fallback deserves a trace.
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll());
        if (parsed.isObject()) {
            doc = parsed.object();
        } else {
            FLOG_WARN(Ch::Settings,
                      "{} did not parse as a JSON object - falling back to "
                      "defaults",
                      settingsFilePath_);
        }
    }
    apply_settings_migrations(doc);
    document_ = doc;
}

bool SettingsHandler::save_document() const
{
    QDir().mkpath(QFileInfo(settingsFilePath_).absolutePath());
    QFile file(settingsFilePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        FLOG_WARN(Ch::Settings,
                  "Could not write settings file {}",
                  settingsFilePath_.toStdString());
        return false;
    }
    file.write(QJsonDocument(document_).toJson(QJsonDocument::Indented));
    return true;
}

QString SettingsHandler::settings_file_path() const
{
    return settingsFilePath_;
}

QJsonValue SettingsHandler::json_value(const QString& group,
                                       const QString& key) const
{
    return document_.value(group).toObject().value(key);
}

void SettingsHandler::set_json_value(const QString& group,
                                     const QString& key,
                                     const QJsonValue& value)
{
    QJsonObject groupObj = document_.value(group).toObject();
    groupObj.insert(key, value);
    document_.insert(group, groupObj);
    save_document();
}

void SettingsHandler::remove_json_value(const QString& group, const QString& key)
{
    QJsonObject groupObj = document_.value(group).toObject();
    groupObj.remove(key);
    document_.insert(group, groupObj);
    save_document();
}

void SettingsHandler::remove_json_group(const QString& group)
{
    document_.remove(group);
    save_document();
}

bool SettingsHandler::json_group_is_empty(const QString& group) const
{
    return document_.value(group).toObject().isEmpty();
}

QStringList SettingsHandler::recent_files_raw() const
{
    QStringList out;
    for (const QJsonValue& v :
         document_.value(QStringLiteral("RecentFiles")).toArray()) {
        out.append(v.toString());
    }
    return out;
}

void SettingsHandler::set_recent_files_raw(const QStringList& files)
{
    QJsonArray arr;
    for (const QString& f : files) {
        arr.append(f);
    }
    document_.insert(QStringLiteral("RecentFiles"), arr);
    save_document();
}

bool SettingsHandler::export_settings_to(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        FLOG_WARN(Ch::Settings,
                  "exportSettingsTo: could not open {}: {}",
                  path,
                  file.errorString());
        return false;
    }
    file.write(QJsonDocument(document_).toJson(QJsonDocument::Indented));
    return true;
}

bool SettingsHandler::import_settings_from(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        FLOG_WARN(Ch::Settings,
                  "importSettingsFrom: could not open {}: {}",
                  path,
                  file.errorString());
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        FLOG_WARN(Ch::Settings,
                  "importSettingsFrom: {} did not parse as a JSON object",
                  path);
        return false;
    }
    QJsonObject obj = doc.object();
    apply_settings_migrations(obj);
    document_ = obj;
    return save_document();
}

void SettingsHandler::set_default_current_preset()
{
    auto currentPreset = current_preset();
    switch (currentPreset) {
    case EPresets::kDarkPreset:
        remove_json_value(QStringLiteral("Colors"),
                          QStringLiteral("dark_color_preset"));
        break;
    case EPresets::kLightPreset:
        remove_json_value(QStringLiteral("Colors"),
                          QStringLiteral("light_color_preset"));
        break;
    case EPresets::kCustom1:
        remove_json_value(QStringLiteral("Colors"),
                          QStringLiteral("custom_preset1"));
        break;
    case EPresets::kCustom2:
        remove_json_value(QStringLiteral("Colors"),
                          QStringLiteral("custom_preset2"));
        break;
    case EPresets::kCustom3:
        remove_json_value(QStringLiteral("Colors"),
                          QStringLiteral("custom_preset3"));
        break;
    case EPresets::kCustom4:
        remove_json_value(QStringLiteral("Colors"),
                          QStringLiteral("custom_preset4"));
        break;
    default:
        break;
    };

    set_current_opacity(255);
}


void SettingsHandler::set_value(const QString& key, const QVariant& value)
{
    FLOG_DEBUG(Ch::Settings, "Setting {} to {}", key, debug_string(value));
    auto val = value_handler(key)->representation(value);
    set_json_value(QStringLiteral("Colors"), key, QJsonValue::fromVariant(val));
}


QVariant SettingsHandler::value(const QString& key) const
{
    const QJsonValue raw = json_value(QStringLiteral("Colors"), key);
    const QVariant val = raw.isUndefined() ? QVariant() : json_to_variant(raw);
    return value_handler(key)->value(val);
}


void SettingsHandler::remove(const QString& key)
{
    remove_json_value(QStringLiteral("Colors"), key);
}


void SettingsHandler::reset_value(const QString& key)
{
    set_json_value(QStringLiteral("Colors"),
                   key,
                   QJsonValue::fromVariant(value_handler(key)->fallback()));
}

SettingsHandler::CL SettingsHandler::get_current_color_preset()
{
    auto currentPreset = current_preset();
    switch (currentPreset) {
    case EPresets::kDarkPreset:
        return dark_color_preset();
    case EPresets::kLightPreset:
        return light_color_preset();
    case EPresets::kCustom1:
        return custom_preset1();
    case EPresets::kCustom2:
        return custom_preset2();
    case EPresets::kCustom3:
        return custom_preset3();
    case EPresets::kCustom4:
        return custom_preset4();
    default:
        break;
    };
    return SettingsHandler::CL{};
}


void SettingsHandler::set_current_color_preset(const SettingsHandler::CL& preset)
{
    auto currentPreset = current_preset();

    switch (currentPreset) {
    case EPresets::kDarkPreset:
        set_dark_color_preset(preset);
        break;
    case EPresets::kLightPreset:
        set_light_color_preset(preset);
        break;
    case EPresets::kCustom1:
        set_custom_preset1(preset);
        break;
    case EPresets::kCustom2:
        set_custom_preset2(preset);
        break;
    case EPresets::kCustom3:
        set_custom_preset3(preset);
        break;
    case EPresets::kCustom4:
        set_custom_preset4(preset);
        break;
    default:
        break;
    };
}

int SettingsHandler::get_current_opacity()
{
    auto currentPreset = current_preset();
    auto masterOpacity = master_opacity();
    return masterOpacity[currentPreset];
}

void SettingsHandler::set_current_opacity(int opacity)
{
    auto currentPreset = current_preset();
    auto masterOpacity = master_opacity();
    masterOpacity[currentPreset] = opacity;
    set_master_opacity(masterOpacity);
}


QSharedPointer<ValueHandler> SettingsHandler::value_handler(const QString& key)
{
    return ::recognizedGeneralOptions.value(key);
}


// ─── Facade: FamSettings-backed ────────────────────────────────────────────────

void SettingsHandler::update_recent_files(const QString& filename)
{
    FamSettings::update_recent_files(filename);
}

QStringList SettingsHandler::get_recent_files(bool existingOnly)
{
    return FamSettings::get_recent_files(existingOnly);
}

QString SettingsHandler::settings_file_name()
{
    return FamSettings::file_name();
}

QVariant SettingsHandler::action_state(const QString& key,
                                       const QVariant& defaultValue)
{
    return FamSettings::value(key, defaultValue);
}

void SettingsHandler::set_action_state(const QString& key, const QVariant& value)
{
    FamSettings::set_value(key, value);
}

qreal SettingsHandler::arrange_gap()
{
    return FamSettings::value_or_default(QStringLiteral("Items/arrange_gap"))
        .toReal();
}

QString SettingsHandler::arrange_default()
{
    return FamSettings::value_or_default(
               QStringLiteral("Items/arrange_default"))
        .toString();
}

QString SettingsHandler::image_storage_format()
{
    return FamSettings::value_or_default(
               QStringLiteral("Items/image_storage_format"))
        .toString();
}

int SettingsHandler::undo_history_size()
{
    return FamSettings::value_or_default(
               QStringLiteral("Items/undo_history_size"))
        .toInt();
}

QString SettingsHandler::auto_optimize_imported_images()
{
    return FamSettings::value_or_default(
               QStringLiteral("Items/auto_optimize_imported_images"))
        .toString();
}

// ─── Facade: KeyboardSettings-backed ───────────────────────────────────────────

std::optional<ControlMatch> SettingsHandler::mousewheel_action_for_event(
    const QWheelEvent* event)
{
    return KeyboardSettings::mousewheel_action_for_event(event);
}

std::optional<ControlMatch> SettingsHandler::mouse_action_for_event(
    const QMouseEvent* event)
{
    return KeyboardSettings::mouse_action_for_event(event);
}

QStringList SettingsHandler::get_shortcuts(const QString& group,
                                           const QString& key,
                                           const QStringList& defaults)
{
    return KeyboardSettings().get_shortcuts(group, key, defaults);
}

void SettingsHandler::set_shortcuts(const QString& group,
                                    const QString& key,
                                    const QStringList& values)
{
    KeyboardSettings::set_shortcuts(group, key, values);
}

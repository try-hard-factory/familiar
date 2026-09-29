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

static QMap<int, int> opacityListDef = {
    {kDarkPreset, 255},
    {kLightPreset, 255},
    {kCustom1, 255},
    {kCustom2, 255},
    {kCustom3, 255},
    {kCustom4, 255},
};

static QMap<int, QColor> darkColorPresetDef
    = {{kBackgroundColor, QColor({32, 32, 32})},   // kBackgroundColor
       {kCanvasColor, QColor({42, 42, 42})},       // kCanvasColor
       {kBorderColor, QColor({13, 13, 13})},       // kBorderColor
       {kTextColor, QColor({255, 255, 255})},      // kTextColor - white
       {kSelectionColor, QColor({22, 142, 153})}}; // kSelectionColor
static QMap<int, QColor> lightColorPresetDef
    = {{kBackgroundColor, QColor({224, 224, 224})},
       {kCanvasColor, QColor({234, 234, 234})},
       {kBorderColor, QColor({200, 200, 200})},
       {kTextColor, QColor({111, 111, 111})},
       {kSelectionColor, QColor({255, 0, 0})}};
static QMap<int, QColor> customPreset1Def
    = {{kBackgroundColor, QColor({32, 32, 32})},
       {kCanvasColor, QColor({42, 42, 42})},
       {kBorderColor, QColor({13, 13, 13})},
       {kTextColor, QColor({122, 122, 122})},
       {kSelectionColor, QColor({22, 142, 153})}};
static QMap<int, QColor> customPreset2Def
    = {{kBackgroundColor, QColor({32, 32, 32})},
       {kCanvasColor, QColor({42, 42, 42})},
       {kBorderColor, QColor({13, 13, 13})},
       {kTextColor, QColor({122, 122, 122})},
       {kSelectionColor, QColor({22, 142, 153})}};
static QMap<int, QColor> customPreset3Def
    = {{kBackgroundColor, QColor({32, 32, 32})},
       {kCanvasColor, QColor({42, 42, 42})},
       {kBorderColor, QColor({13, 13, 13})},
       {kTextColor, QColor({122, 122, 122})},
       {kSelectionColor, QColor({22, 142, 153})}};
static QMap<int, QColor> customPreset4Def
    = {{kBackgroundColor, QColor({32, 32, 32})},
       {kCanvasColor, QColor({42, 42, 42})},
       {kBorderColor, QColor({13, 13, 13})},
       {kTextColor, QColor({122, 122, 122})},
       {kSelectionColor, QColor({22, 142, 153})}};

static QMap<class QString, QSharedPointer<ValueHandler>> recognizedGeneralOptions
    = {
        //         KEY                            TYPE                 DEFAULT_VALUE
        OPTION("option0", Bool(true)),
        OPTION("option1", Bool(true)),
        OPTION("currentPreset",
               BoundedInt(0, EPresets::kAllPresets, EPresets::kDarkPreset)),
        OPTION("masterOpacity", OpacityList(opacityListDef)),
        OPTION("darkColorPreset", ColorList(darkColorPresetDef)),
        OPTION("lightColorPreset", ColorList(lightColorPresetDef)),
        OPTION("customPreset1", ColorList(customPreset1Def)),
        OPTION("customPreset2", ColorList(customPreset2Def)),
        OPTION("customPreset3", ColorList(customPreset3Def)),
        OPTION("customPreset4", ColorList(customPreset4Def)),

};

namespace {

// QJsonValue::toVariant() doesn't guarantee int over double for whole
// numbers, but ValueHandler::check() implementations (e.g. BoundedInt)
// go through QVariant::toString().toInt() - safest to pin whole numbers
// down to a real int right at the JSON/QVariant boundary, once, here.
QVariant jsonToVariant(const QJsonValue& v)
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
constexpr int kSettingsSchemaVersion = 1;
constexpr char kSchemaVersionKey[] = "schemaVersion";

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
const QMap<int, std::function<void(QJsonObject&)>>& settingsMigrations()
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
void applySettingsMigrations(QJsonObject& doc)
{
    int version = doc.value(QLatin1String(kSchemaVersionKey))
                      .toInt(kSettingsSchemaVersion);
    if (version > kSettingsSchemaVersion) {
        FLOG_WARN(Ch::Settings,
                  "settings.json has schemaVersion {} - newer than this "
                  "build supports ({}) - loading best-effort",
                  version,
                  kSettingsSchemaVersion);
        return;
    }
    const auto& migrations = settingsMigrations();
    while (version < kSettingsSchemaVersion) {
        auto it = migrations.find(version);
        if (it != migrations.end()) {
            it.value()(doc);
        }
        ++version;
    }
    doc[QLatin1String(kSchemaVersionKey)] = kSettingsSchemaVersion;
}

} // namespace

SettingsHandler::SettingsHandler()
{
    settingsFilePath_ = CommandlineArgs::instance().settingsFile();
    if (settingsFilePath_.isEmpty()) {
        QString dir = portableDataDir();
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
    applySettingsMigrations(doc);
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
    applySettingsMigrations(obj);
    document_ = obj;
    return save_document();
}

void SettingsHandler::set_default_current_preset()
{
    auto current_preset = currentPreset();
    switch (current_preset) {
    case EPresets::kDarkPreset:
        remove_json_value(QStringLiteral("Colors"),
                        QStringLiteral("darkColorPreset"));
        break;
    case EPresets::kLightPreset:
        remove_json_value(QStringLiteral("Colors"),
                        QStringLiteral("lightColorPreset"));
        break;
    case EPresets::kCustom1:
        remove_json_value(QStringLiteral("Colors"),
                        QStringLiteral("customPreset1"));
        break;
    case EPresets::kCustom2:
        remove_json_value(QStringLiteral("Colors"),
                        QStringLiteral("customPreset2"));
        break;
    case EPresets::kCustom3:
        remove_json_value(QStringLiteral("Colors"),
                        QStringLiteral("customPreset3"));
        break;
    case EPresets::kCustom4:
        remove_json_value(QStringLiteral("Colors"),
                        QStringLiteral("customPreset4"));
        break;
    default:
        break;
    };

    set_current_opacity(255);
}


void SettingsHandler::setValue(const QString& key, const QVariant& value)
{
    FLOG_DEBUG(Ch::Settings, "Setting {} to {}", key, debugString(value));
    auto val = value_handler(key)->representation(value);
    set_json_value(QStringLiteral("Colors"), key, QJsonValue::fromVariant(val));
}


QVariant SettingsHandler::value(const QString& key) const
{
    const QJsonValue raw = json_value(QStringLiteral("Colors"), key);
    const QVariant val = raw.isUndefined() ? QVariant() : jsonToVariant(raw);
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
    auto current_preset = currentPreset();
    switch (current_preset) {
    case EPresets::kDarkPreset:
        return darkColorPreset();
    case EPresets::kLightPreset:
        return lightColorPreset();
    case EPresets::kCustom1:
        return customPreset1();
    case EPresets::kCustom2:
        return customPreset2();
    case EPresets::kCustom3:
        return customPreset3();
    case EPresets::kCustom4:
        return customPreset4();
    default:
        break;
    };
    return SettingsHandler::CL{};
}


void SettingsHandler::set_current_color_preset(const SettingsHandler::CL& preset)
{
    auto current_preset = currentPreset();

    switch (current_preset) {
    case EPresets::kDarkPreset:
        setDarkColorPreset(preset);
        break;
    case EPresets::kLightPreset:
        setLightColorPreset(preset);
        break;
    case EPresets::kCustom1:
        setCustomPreset1(preset);
        break;
    case EPresets::kCustom2:
        setCustomPreset2(preset);
        break;
    case EPresets::kCustom3:
        setCustomPreset3(preset);
        break;
    case EPresets::kCustom4:
        setCustomPreset4(preset);
        break;
    default:
        break;
    };
}

int SettingsHandler::get_current_opacity()
{
    auto current_preset = currentPreset();
    auto master_opacity = masterOpacity();
    return master_opacity[current_preset];
}

void SettingsHandler::set_current_opacity(int opacity)
{
    auto current_preset = currentPreset();
    auto master_opacity = masterOpacity();
    master_opacity[current_preset] = opacity;
    setMasterOpacity(master_opacity);
}


QSharedPointer<ValueHandler> SettingsHandler::value_handler(
    const QString& key) const
{
    return ::recognizedGeneralOptions.value(key);
}


// ─── Facade: FamSettings-backed ────────────────────────────────────────────────

void SettingsHandler::update_recent_files(const QString& filename)
{
    FamSettings().updateRecentFiles(filename);
}

QStringList SettingsHandler::get_recent_files(bool existingOnly) const
{
    return FamSettings().getRecentFiles(existingOnly);
}

QString SettingsHandler::settings_file_name() const
{
    return FamSettings().fileName();
}

QVariant SettingsHandler::action_state(const QString& key,
                                      const QVariant& defaultValue) const
{
    return FamSettings().value(key, defaultValue);
}

void SettingsHandler::set_action_state(const QString& key, const QVariant& value)
{
    FamSettings().setValue(key, value);
}

qreal SettingsHandler::arrange_gap() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/arrange_gap"))
        .toReal();
}

QString SettingsHandler::arrange_default() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/arrange_default"))
        .toString();
}

QString SettingsHandler::image_storage_format() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/image_storage_format"))
        .toString();
}

int SettingsHandler::undo_history_size() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/undo_history_size"))
        .toInt();
}

QString SettingsHandler::auto_optimize_imported_images() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/auto_optimize_imported_images"))
        .toString();
}

// ─── Facade: KeyboardSettings-backed ───────────────────────────────────────────

std::optional<ControlMatch> SettingsHandler::mousewheel_action_for_event(
    const QWheelEvent* event) const
{
    return KeyboardSettings().mousewheel_action_for_event(event);
}

std::optional<ControlMatch> SettingsHandler::mouse_action_for_event(
    const QMouseEvent* event) const
{
    return KeyboardSettings().mouse_action_for_event(event);
}

QStringList SettingsHandler::get_shortcuts(const QString& group,
                                          const QString& key,
                                          const QStringList& defaults) const
{
    return KeyboardSettings().get_shortcuts(group, key, defaults);
}

void SettingsHandler::set_shortcuts(const QString& group,
                                   const QString& key,
                                   const QStringList& values)
{
    KeyboardSettings().set_shortcuts(group, key, values);
}

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
       {kTextColor, QColor({122, 122, 122})},      // kTextColor
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
                   <= static_cast<double>(std::numeric_limits<int>::max()))
            return static_cast<int>(d);
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
        if (it != migrations.end())
            it.value()(doc);
        ++version;
    }
    doc[QLatin1String(kSchemaVersionKey)] = kSettingsSchemaVersion;
}

} // namespace

SettingsHandler::SettingsHandler()
{
    settingsFilePath_ = CommandlineArgs::instance().settingsFile();
    if (settingsFilePath_.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation);
        settingsFilePath_ = QDir(dir).filePath(QStringLiteral("settings.json"));
    }
    loadDocument();
}


SettingsHandler* SettingsHandler::getInstance()
{
    static SettingsHandler config;
    return &config;
}

void SettingsHandler::loadDocument()
{
    QFile file(settingsFilePath_);
    QJsonObject doc;
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll());
        if (parsed.isObject())
            doc = parsed.object();
    }
    applySettingsMigrations(doc);
    document_ = doc;
}

bool SettingsHandler::saveDocument() const
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

QString SettingsHandler::settingsFilePath() const
{
    return settingsFilePath_;
}

QJsonValue SettingsHandler::jsonValue(const QString& group,
                                      const QString& key) const
{
    return document_.value(group).toObject().value(key);
}

void SettingsHandler::setJsonValue(const QString& group,
                                   const QString& key,
                                   const QJsonValue& value)
{
    QJsonObject groupObj = document_.value(group).toObject();
    groupObj.insert(key, value);
    document_.insert(group, groupObj);
    saveDocument();
}

void SettingsHandler::removeJsonValue(const QString& group, const QString& key)
{
    QJsonObject groupObj = document_.value(group).toObject();
    groupObj.remove(key);
    document_.insert(group, groupObj);
    saveDocument();
}

void SettingsHandler::removeJsonGroup(const QString& group)
{
    document_.remove(group);
    saveDocument();
}

QStringList SettingsHandler::recentFilesRaw() const
{
    QStringList out;
    for (const QJsonValue& v :
         document_.value(QStringLiteral("RecentFiles")).toArray())
        out.append(v.toString());
    return out;
}

void SettingsHandler::setRecentFilesRaw(const QStringList& files)
{
    QJsonArray arr;
    for (const QString& f : files)
        arr.append(f);
    document_.insert(QStringLiteral("RecentFiles"), arr);
    saveDocument();
}

bool SettingsHandler::exportSettingsTo(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(document_).toJson(QJsonDocument::Indented));
    return true;
}

bool SettingsHandler::importSettingsFrom(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    QJsonObject obj = doc.object();
    applySettingsMigrations(obj);
    document_ = obj;
    return saveDocument();
}

void SettingsHandler::setDefaultCurrentPreset()
{
    auto current_preset = currentPreset();
    switch (current_preset) {
    case EPresets::kDarkPreset:
        removeJsonValue(QStringLiteral("Colors"),
                        QStringLiteral("darkColorPreset"));
        break;
    case EPresets::kLightPreset:
        removeJsonValue(QStringLiteral("Colors"),
                        QStringLiteral("lightColorPreset"));
        break;
    case EPresets::kCustom1:
        removeJsonValue(QStringLiteral("Colors"),
                        QStringLiteral("customPreset1"));
        break;
    case EPresets::kCustom2:
        removeJsonValue(QStringLiteral("Colors"),
                        QStringLiteral("customPreset2"));
        break;
    case EPresets::kCustom3:
        removeJsonValue(QStringLiteral("Colors"),
                        QStringLiteral("customPreset3"));
        break;
    case EPresets::kCustom4:
        removeJsonValue(QStringLiteral("Colors"),
                        QStringLiteral("customPreset4"));
        break;
    default:
        break;
    };

    setCurrentOpacity(255);
}


void SettingsHandler::setValue(const QString& key, const QVariant& value)
{
    FLOG_DEBUG(Ch::Settings, "Setting {} to {}", key, debugString(value));
    auto val = valueHandler(key)->representation(value);
    setJsonValue(QStringLiteral("Colors"), key, QJsonValue::fromVariant(val));
}


QVariant SettingsHandler::value(const QString& key) const
{
    const QJsonValue raw = jsonValue(QStringLiteral("Colors"), key);
    const QVariant val = raw.isUndefined() ? QVariant() : jsonToVariant(raw);
    return valueHandler(key)->value(val);
}


void SettingsHandler::remove(const QString& key)
{
    removeJsonValue(QStringLiteral("Colors"), key);
}


void SettingsHandler::resetValue(const QString& key)
{
    setJsonValue(QStringLiteral("Colors"),
                 key,
                 QJsonValue::fromVariant(valueHandler(key)->fallback()));
}

SettingsHandler::CL SettingsHandler::getCurrentColorPreset()
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


void SettingsHandler::setCurrentColorPreset(const SettingsHandler::CL& preset)
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

int SettingsHandler::getCurrentOpacity()
{
    auto current_preset = currentPreset();
    auto master_opacity = masterOpacity();
    return master_opacity[current_preset];
}

void SettingsHandler::setCurrentOpacity(int opacity)
{
    auto current_preset = currentPreset();
    auto master_opacity = masterOpacity();
    master_opacity[current_preset] = opacity;
    setMasterOpacity(master_opacity);
}


QSharedPointer<ValueHandler> SettingsHandler::valueHandler(
    const QString& key) const
{
    return ::recognizedGeneralOptions.value(key);
}


// ─── Facade: FamSettings-backed ────────────────────────────────────────────────

void SettingsHandler::updateRecentFiles(const QString& filename)
{
    FamSettings().updateRecentFiles(filename);
}

QStringList SettingsHandler::getRecentFiles(bool existingOnly) const
{
    return FamSettings().getRecentFiles(existingOnly);
}

QString SettingsHandler::settingsFileName() const
{
    return FamSettings().fileName();
}

QVariant SettingsHandler::actionState(const QString& key,
                                      const QVariant& defaultValue) const
{
    return FamSettings().value(key, defaultValue);
}

void SettingsHandler::setActionState(const QString& key, const QVariant& value)
{
    FamSettings().setValue(key, value);
}

qreal SettingsHandler::arrangeGap() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/arrange_gap"))
        .toReal();
}

QString SettingsHandler::arrangeDefault() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/arrange_default"))
        .toString();
}

QString SettingsHandler::imageStorageFormat() const
{
    return FamSettings()
        .valueOrDefault(QStringLiteral("Items/image_storage_format"))
        .toString();
}

// ─── Facade: KeyboardSettings-backed ───────────────────────────────────────────

std::optional<ControlMatch> SettingsHandler::mousewheelActionForEvent(
    const QWheelEvent* event) const
{
    return KeyboardSettings().mousewheelActionForEvent(event);
}

std::optional<ControlMatch> SettingsHandler::mouseActionForEvent(
    const QMouseEvent* event) const
{
    return KeyboardSettings().mouseActionForEvent(event);
}

QStringList SettingsHandler::getShortcuts(const QString& group,
                                          const QString& key,
                                          const QStringList& defaults) const
{
    return KeyboardSettings().get_shortcuts(group, key, defaults);
}

void SettingsHandler::setShortcuts(const QString& group,
                                   const QString& key,
                                   const QStringList& values)
{
    KeyboardSettings().setShortcuts(group, key, values);
}

#ifndef SETTINGSHANDLER_H
#define SETTINGSHANDLER_H

#include "core/controls.h"

#include <optional>

#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QVector>

// TODOLATER: rework this class and FamSettings someday...

class QWheelEvent;
class QMouseEvent;

enum EPresets {
    kDarkPreset = 0,
    kLightPreset = 1,
    kCustom1 = 2,
    kCustom2 = 3,
    kCustom3 = 4,
    kCustom4 = 5,
    kAllPresets = 6
};

enum EPresetsColorIdx {
    kBackgroundColor = 0,
    kCanvasColor = 1,
    kBorderColor = 2,
    kTextColor = 3,
    kSelectionColor = 4,
    kAllIdx = 5
};

class ValueHandler;
template<class T>
class QSharedPointer;

#define SETTINGS_GETTER(KEY, TYPE) \
    TYPE KEY() \
    { \
        return value(QStringLiteral(#KEY)).value<TYPE>(); \
    }
#define SETTINGS_SETTER(FUNC, KEY, TYPE) \
    void FUNC(const TYPE& val) \
    { \
        QString key = QStringLiteral(#KEY); \
        if (QVariant::fromValue(val) != value(key)) { \
            setValue(key, QVariant::fromValue(val)); \
        } \
    }
#define SETTINGS_GETTER_SETTER(GETFUNC, SETFUNC, TYPE) \
    SETTINGS_GETTER(GETFUNC, TYPE) \
    SETTINGS_SETTER(SETFUNC, GETFUNC, TYPE)


class SettingsHandler : public QObject
{
    Q_OBJECT

public:
    explicit SettingsHandler();
    static SettingsHandler* getInstance();

    // GENERIC GETTERS AND SETTERS
    SETTINGS_GETTER_SETTER(currentPreset, setCurrentPreset, int)
    using OL = QMap<int, int>;
    SETTINGS_GETTER_SETTER(masterOpacity, setMasterOpacity, OL)
    using CL = QMap<int, QColor>;
    SETTINGS_GETTER_SETTER(darkColorPreset, setDarkColorPreset, CL)
    SETTINGS_GETTER_SETTER(lightColorPreset, setLightColorPreset, CL)
    SETTINGS_GETTER_SETTER(customPreset1, setCustomPreset1, CL)
    SETTINGS_GETTER_SETTER(customPreset2, setCustomPreset2, CL)
    SETTINGS_GETTER_SETTER(customPreset3, setCustomPreset3, CL)
    SETTINGS_GETTER_SETTER(customPreset4, setCustomPreset4, CL)

    void setDefaultCurrentPreset();

    // These back onto the "Colors" JSON group via valueHandler()'s
    // check/process/fallback/representation (core/valuehandler.h) - kept
    // as the stable entry point SETTINGS_GETTER_SETTER expands into.
    // TODOLATER:
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key) const;
    void remove(const QString& key);
    void resetValue(const QString& key);

    CL getCurrentColorPreset();
    void setCurrentColorPreset(const CL& preset);
    int getCurrentOpacity();
    void setCurrentOpacity(int opacity);

    // ── The single JSON settings file (core/settingshandler.cpp) ──────────────
    // Every other settings-adjacent class (FamSettings/KeyboardSettings,
    // core/settings.h / core/controls.h) funnels its group/key reads and
    // writes through these instead of touching disk itself - this is the
    // only class that actually owns the file. Group names are just the
    // group half of the flat "Group/key" strings those classes already
    // used with QSettings, now real JSON nesting instead of a "/"-joined
    // prefix.
    QString settingsFilePath() const;
    QJsonValue jsonValue(const QString& group, const QString& key) const;
    void setJsonValue(const QString& group,
                      const QString& key,
                      const QJsonValue& value);
    void removeJsonValue(const QString& group, const QString& key);
    void removeJsonGroup(const QString& group);
    // True if `group` has no stored keys at all (missing entirely, or
    // present but empty) - i.e. "nothing here differs from the code
    // defaults". Used by RestoreDefaultsDialog to pre-check only the
    // categories that actually have something to restore.
    bool jsonGroupIsEmpty(const QString& group) const;
    QStringList recentFilesRaw() const;
    void setRecentFilesRaw(const QStringList& files);

    // Import replaces the live document (and persists it) without
    // clearing anything first, unlike restoreDefaults() - the imported
    // values ARE the new state, not a reason to fall back to defaults.
    // Export just writes the current document out to a second location.
    bool exportSettingsTo(const QString& path) const;
    bool importSettingsFrom(const QString& path);

    // FACADE: SettingsHandler is the only settings class code outside
    // the settings subsystem itself (core/, widgets/controls/,
    // widgets/setting_row.*) should call. These delegate to
    // FamSettings/KeyboardSettings (core/settings.h, core/controls.h),
    // which stay the internal storage-shaped API - not reimplemented here.

    // FamSettings-backed
    void updateRecentFiles(const QString& filename);
    QStringList getRecentFiles(bool existingOnly = false) const;
    QString settingsFileName() const;
    // Generic per-action persisted checkbox state (Action::settingsKey,
    // see actions/action_mixin.h) - not one of FamSettings::fields()'
    // named keys, so no dedicated accessor makes sense.
    QVariant actionState(const QString& key, const QVariant& defaultValue) const;
    void setActionState(const QString& key, const QVariant& value);
    qreal arrangeGap() const;
    QString arrangeDefault() const;
    QString imageStorageFormat() const;
    int undoHistorySize() const;
    QString autoOptimizeImportedImages() const;

    // KeyboardSettings-backed (also the underlying store for mouse/wheel
    // control matching, despite the class name)
    std::optional<ControlMatch> mousewheelActionForEvent(
        const QWheelEvent* event) const;
    std::optional<ControlMatch> mouseActionForEvent(
        const QMouseEvent* event) const;
    QStringList getShortcuts(const QString& group,
                             const QString& key,
                             const QStringList& defaults = {}) const;
    void setShortcuts(const QString& group,
                      const QString& key,
                      const QStringList& values);


signals:
    void settingsChanged() const;
    void presetsChanged() const;

private:
    QSharedPointer<ValueHandler> valueHandler(const QString& key) const;
    void loadDocument();
    bool saveDocument() const;

private:
    QString settingsFilePath_;
    QJsonObject document_;
};

#endif // SETTINGSHANDLER_H

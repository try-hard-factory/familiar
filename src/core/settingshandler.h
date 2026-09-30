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

// The settings.json key IS the getter's name, stringified. Renaming a
// getter therefore renames the persisted key - keep the OPTION() entries
// in recognizedGeneralOptions (settingshandler.cpp) and the literals in
// set_default_current_preset() / restore_defaults_dialog.cpp in step with
// it, or value_handler() hands back a null handler and
// SettingsHandler::value() dereferences it.
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
    static SettingsHandler* get_instance();

    // GENERIC GETTERS AND SETTERS
    SETTINGS_GETTER_SETTER(current_preset, set_current_preset, int)
    using OL = QMap<int, int>;
    SETTINGS_GETTER_SETTER(master_opacity, set_master_opacity, OL)
    using CL = QMap<int, QColor>;
    SETTINGS_GETTER_SETTER(dark_color_preset, set_dark_color_preset, CL)
    SETTINGS_GETTER_SETTER(light_color_preset, set_light_color_preset, CL)
    SETTINGS_GETTER_SETTER(custom_preset1, set_custom_preset1, CL)
    SETTINGS_GETTER_SETTER(custom_preset2, set_custom_preset2, CL)
    SETTINGS_GETTER_SETTER(custom_preset3, set_custom_preset3, CL)
    SETTINGS_GETTER_SETTER(custom_preset4, set_custom_preset4, CL)

    void set_default_current_preset();

    // These back onto the "Colors" JSON group via valueHandler()'s
    // check/process/fallback/representation (core/valuehandler.h) - kept
    // as the stable entry point SETTINGS_GETTER_SETTER expands into.
    // TODOLATER: + name change
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key) const;
    void remove(const QString& key);
    void reset_value(const QString& key);

    CL get_current_color_preset();
    void set_current_color_preset(const CL& preset);
    int get_current_opacity();
    void set_current_opacity(int opacity);

    // ── The single JSON settings file (core/settingshandler.cpp) ──────────────
    // Every other settings-adjacent class (FamSettings/KeyboardSettings,
    // core/settings.h / core/controls.h) funnels its group/key reads and
    // writes through these instead of touching disk itself - this is the
    // only class that actually owns the file. Group names are just the
    // group half of the flat "Group/key" strings those classes already
    // used with QSettings, now real JSON nesting instead of a "/"-joined
    // prefix.
    QString settings_file_path() const;
    QJsonValue json_value(const QString& group, const QString& key) const;
    void set_json_value(const QString& group,
                      const QString& key,
                      const QJsonValue& value);
    void remove_json_value(const QString& group, const QString& key);
    void remove_json_group(const QString& group);
    // True if `group` has no stored keys at all (missing entirely, or
    // present but empty) - i.e. "nothing here differs from the code
    // defaults". Used by RestoreDefaultsDialog to pre-check only the
    // categories that actually have something to restore.
    bool json_group_is_empty(const QString& group) const;
    QStringList recent_files_raw() const;
    void set_recent_files_raw(const QStringList& files);

    // Import replaces the live document (and persists it) without
    // clearing anything first, unlike restoreDefaults() - the imported
    // values ARE the new state, not a reason to fall back to defaults.
    // Export just writes the current document out to a second location.
    bool export_settings_to(const QString& path) const;
    bool import_settings_from(const QString& path);

    // FACADE: SettingsHandler is the only settings class code outside
    // the settings subsystem itself (core/, widgets/controls/,
    // widgets/setting_row.*) should call. These delegate to
    // FamSettings/KeyboardSettings (core/settings.h, core/controls.h),
    // which stay the internal storage-shaped API - not reimplemented here.

    // FamSettings-backed
    void update_recent_files(const QString& filename);
    QStringList get_recent_files(bool existingOnly = false) const;
    QString settings_file_name() const;
    // Generic per-action persisted checkbox state (Action::settingsKey,
    // see actions/action_mixin.h) - not one of FamSettings::fields()'
    // named keys, so no dedicated accessor makes sense.
    QVariant action_state(const QString& key, const QVariant& defaultValue) const;
    void set_action_state(const QString& key, const QVariant& value);
    qreal arrange_gap() const;
    QString arrange_default() const;
    QString image_storage_format() const;
    int undo_history_size() const;
    QString auto_optimize_imported_images() const;

    // KeyboardSettings-backed (also the underlying store for mouse/wheel
    // control matching, despite the class name)
    std::optional<ControlMatch> mousewheel_action_for_event(
        const QWheelEvent* event) const;
    std::optional<ControlMatch> mouse_action_for_event(
        const QMouseEvent* event) const;
    QStringList get_shortcuts(const QString& group,
                             const QString& key,
                             const QStringList& defaults = {}) const;
    void set_shortcuts(const QString& group,
                      const QString& key,
                      const QStringList& values);


signals:
    void settings_changed() const;
    void presets_changed() const;

private:
    QSharedPointer<ValueHandler> value_handler(const QString& key) const;
    void load_document();
    bool save_document() const;

private:
    QString settingsFilePath_;
    QJsonObject document_;
};

#endif // SETTINGSHANDLER_H

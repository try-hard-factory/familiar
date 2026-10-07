#ifndef SETTINGS_H
#define SETTINGS_H

#include <functional>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QVariant>

class QCoreApplication;

// ─── CommandlineArgs ──────────────────────────────────────────────────────────

class CommandlineArgs
{
public:
    static CommandlineArgs& instance();

    // Call once from main() after constructing QApplication, before anything else.
    void process(const QCoreApplication& app);

    // For unit tests: parse without exiting on unknown args.
    void parse(const QStringList& args);

    QString filename() const { return filename_; }
    // JSON settings file to read/write instead of the default location
    // (QStandardPaths::AppConfigLocation + "settings.json") - see
    // SettingsHandler::SettingsHandler() (core/settingshandler.cpp).
    QString settings_file() const { return settingsFile_; }
    QString loglevel() const { return loglevel_; }
    bool debug_bounding_rects() const { return debugBoundingRects_; }
    bool debug_shapes() const { return debugShapes_; }
    bool debug_handles() const { return debugHandles_; }

private:
    CommandlineArgs() = default;

    QString filename_;
    QString settingsFile_;
    QString loglevel_ = QStringLiteral("INFO");
    bool debugBoundingRects_ = false;
    bool debugShapes_ = false;
    bool debugHandles_ = false;
};

// ─── SettingsEvents ───────────────────────────────────────────────────────────
// Global signal bus for settings state changes.
// Equivalent to Python's BeeSettingsEvents / settings_events singleton.

class SettingsEvents : public QObject
{
    Q_OBJECT
public:
    static SettingsEvents& instance();

signals:
    void restore_defaults();
    void restore_keyboard_defaults();
    // Fired from both Save/autosave_enabled's and
    // Save/autosave_interval_seconds' postSaveCallback so MainWindow's
    // autosave QTimer can pick up either change live, instead of only on
    // next launch (see FamSettings::setValue()). No payload - the
    // listener just re-reads both settings fresh, since either one alone
    // doesn't have the other's current value.
    void autosave_settings_changed();

private:
    SettingsEvents() = default;
};

// ─── FamSettings ─────────────────────────────────────────────────────────────

struct FieldConfig
{
    QVariant defaultValue;
    // Optional type cast applied before validation.
    std::function<QVariant(const QVariant&)> cast;
    // Optional semantic validation; return false → fall back to default.
    std::function<bool(const QVariant&)> validate;
    // Optional callback fired after every setValue / remove for this key.
    std::function<void(const QVariant&)> postSaveCallback;
};

// Thin value-typed facade, constructed fresh at each call site
// (`FamSettings settings; settings.valueOrDefault(key)`) same as before -
// holds no state of its own. Actual storage is the single JSON document
// owned by SettingsHandler (core/settingshandler.h); every key here still
// looks like "Group/subkey" (e.g. "Save/confirm_close_unsaved") and gets
// split on the first '/' into a JSON group + subkey underneath.
class FamSettings
{
public:
    static const QMap<QString, FieldConfig>& fields();

    // Returns stored value with cast + validation applied; falls back to default.
    static QVariant value_or_default(const QString& key);

    // Returns true if stored value differs from FIELDS default.
    static bool value_changed(const QString& key);

    // Remove all FIELDS keys from storage and emit SettingsEvents::restoreDefaults.
    static void restore_defaults();

    // Apply startup-time settings (e.g. image allocation limit).
    static void on_startup();

    // Fires postSaveCallback when defined for `key`.
    static void set_value(const QString& key, const QVariant& value);

    // Raw read for keys that aren't in fields() (e.g. Action::settingsKey's
    // ad-hoc checkbox state, written via setValue() above) - same
    // "Group/subkey" splitting as valueOrDefault(), just without the
    // fields()-driven cast/validate/default.
    static QVariant value(const QString& key, const QVariant& defaultValue);

    // Fires postSaveCallback when defined for `key`.
    static void remove(const QString& key);

    static void update_recent_files(const QString& filename);
    static QStringList get_recent_files(bool existingOnly = false);
    static QString file_name();
};

#endif // SETTINGS_H

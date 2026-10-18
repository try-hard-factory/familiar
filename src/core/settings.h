#ifndef SETTINGS_H
#define SETTINGS_H

#include <functional>
#include <optional>
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

    // Call once from main() after constructing QApplication, before anything
    // else. Returns the exit code main() should return for the "print and
    // quit" arguments (--help/--version/a parse error), or nullopt to carry
    // on starting up.
    //
    // Deliberately NOT std::exit() from in here, which is what this used to
    // do: exit() skips every local destructor in main(), including the
    // single-instance QSharedMemory guard's. Measured, not theoretical -
    // every --help/--version left a System V segment behind (`ipcs -m`,
    // nattch 0 and no `dest` flag, so not even marked for destruction). One
    // at a time: the next launch's attach()-then-detach() does clear the
    // previous one (see acquire_single_instance_lock() in main.cpp).
    [[nodiscard]] std::optional<int> process(const QCoreApplication& app);

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

// Upper bounds of the numeric settings. Declared here because they are
// needed in TWO places that have to agree: the validate() lambdas in
// core/settings.cpp's field table, and the spin-box ranges in
// widgets/setting_row.cpp. They used to be separate literals in each -
// had they ever drifted apart, the UI would have happily offered a value
// its own validator then rejected, and value_or_default() would silently
// hand back the default instead.
//
// Namespaced because this header is included widely and bare names like
// kmaxArrangeGap would leak into every translation unit.
namespace setting_limits {

constexpr int kmaxArrangeGap = 200;
constexpr int kmaxImageSizeMb = 1024;
constexpr int kmaxAutosaveIntervalSeconds = 60 * 60;
// No upper bound in the validator (see core/settings.cpp); this is only
// what the spin box offers.
constexpr int kmaxUndoHistory = 10000;

// How many recent files are kept, and therefore how many entries the
// Recent Files submenu builds. Same two-places problem as the bounds
// above: FamSettings::update_recent_files() trims the stored list to this
// length, and ActionsMixin::build_recent_files() (actions/action_mixin.h)
// loops exactly this many times to create the menu actions - a mismatch
// would either hide stored entries or add dead ones.
constexpr int kmaxRecentFiles = 10;

} // namespace setting_limits

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

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
    QString settingsFile() const { return settingsFile_; }
    QString loglevel() const { return loglevel_; }
    bool debugBoundingRects() const { return debugBoundingRects_; }
    bool debugShapes() const { return debugShapes_; }
    bool debugHandles() const { return debugHandles_; }

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
    void restoreDefaults();
    void restoreKeyboardDefaults();

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
    QVariant valueOrDefault(const QString& key) const;

    // Returns true if stored value differs from FIELDS default.
    bool valueChanged(const QString& key) const;

    // Remove all FIELDS keys from storage and emit SettingsEvents::restoreDefaults.
    void restoreDefaults();

    // Apply startup-time settings (e.g. image allocation limit).
    void onStartup();

    // Fires postSaveCallback when defined for `key`.
    void setValue(const QString& key, const QVariant& value);

    // Raw read for keys that aren't in fields() (e.g. Action::settingsKey's
    // ad-hoc checkbox state, written via setValue() above) - same
    // "Group/subkey" splitting as valueOrDefault(), just without the
    // fields()-driven cast/validate/default.
    QVariant value(const QString& key, const QVariant& defaultValue) const;

    // Fires postSaveCallback when defined for `key`.
    void remove(const QString& key);

    void updateRecentFiles(const QString& filename);
    QStringList getRecentFiles(bool existingOnly = false) const;

    QString fileName() const;
};

QString logfileName();

#endif // SETTINGS_H

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariant>
#include <functional>

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
    QString settingsDir() const { return settingsDir_; }
    QString loglevel() const { return loglevel_; }
    bool debugBoundingRects() const { return debugBoundingRects_; }
    bool debugShapes() const { return debugShapes_; }
    bool debugHandles() const { return debugHandles_; }

private:
    CommandlineArgs() = default;

    QString filename_;
    QString settingsDir_;
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

struct FieldConfig {
    QVariant defaultValue;
    // Optional type cast applied before validation.
    std::function<QVariant(const QVariant&)> cast;
    // Optional semantic validation; return false → fall back to default.
    std::function<bool(const QVariant&)> validate;
    // Optional callback fired after every setValue / remove for this key.
    std::function<void(const QVariant&)> postSaveCallback;
};

class FamSettings : public QSettings
{
public:
    FamSettings();
    static FamSettings* getInstance();
    static const QMap<QString, FieldConfig>& fields();

    // Returns stored value with cast + validation applied; falls back to default.
    QVariant valueOrDefault(const QString& key) const;

    // Returns true if stored value differs from FIELDS default.
    bool valueChanged(const QString& key) const;

    // Remove all FIELDS keys from storage and emit SettingsEvents::restoreDefaults.
    void restoreDefaults();

    // Apply startup-time settings (e.g. image allocation limit).
    void onStartup();

    // Shadowing QSettings::setValue to fire postSaveCallback when defined.
    void setValue(const QString& key, const QVariant& value);

    // Shadowing QSettings::remove to fire postSaveCallback when defined.
    void remove(const QString& key);

    void updateRecentFiles(const QString& filename);
    QStringList getRecentFiles(bool existingOnly = false) const;

private:
    static QSettings::Format initPathAndReturnFormat();
};

QString logfileName();

#endif // SETTINGS_H

#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QStringList>

class QCoreApplication;

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

class FamSettings : public QSettings
{
public:
    FamSettings();

    void updateRecentFiles(const QString& filename);
    QStringList getRecentFiles(bool existingOnly = false) const;

private:
    static QSettings::Format initPathAndReturnFormat();
};

class KeyboardSettings : public QSettings
{
public:
    KeyboardSettings();

    void setShortcuts(const QString& group, const QString& key, const QStringList& values);
    QStringList getShortcuts(const QString& group, const QString& key,
                             const QStringList& defaultValues = {});

    bool saveUnknownShortcuts = true;
};

QString logfileName();

#endif // CONFIG_H

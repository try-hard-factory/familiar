#include "settings.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

// TODO:

// ─── CommandlineArgs ──────────────────────────────────────────────────────────

CommandlineArgs& CommandlineArgs::instance()
{
    static CommandlineArgs inst;
    return inst;
}

static void addOptions(QCommandLineParser& parser)
{
    parser.addPositionalArgument(
        QStringLiteral("filename"),
        QCoreApplication::tr("Bee file to open"),
        QStringLiteral("[filename]"));

    parser.addOption({QStringLiteral("settings-dir"),
                      QCoreApplication::tr("Settings directory to use instead of default location"),
                      QStringLiteral("dir")});

    parser.addOption({QStringList{QStringLiteral("l"), QStringLiteral("loglevel")},
                      QCoreApplication::tr("Log level for console output"),
                      QStringLiteral("level"),
                      QStringLiteral("INFO")});

    parser.addOption({QStringLiteral("debug-boundingrects"),
                      QCoreApplication::tr("Draw item's bounding rects for debugging")});

    parser.addOption({QStringLiteral("debug-shapes"),
                      QCoreApplication::tr("Draw item's mouse event shapes for debugging")});

    parser.addOption({QStringLiteral("debug-handles"),
                      QCoreApplication::tr("Draw item's transform handle areas for debugging")});
}

void CommandlineArgs::process(const QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    addOptions(parser);
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        filename_ = positional.first();

    settingsDir_        = parser.value(QStringLiteral("settings-dir"));
    loglevel_           = parser.value(QStringLiteral("loglevel"));
    debugBoundingRects_ = parser.isSet(QStringLiteral("debug-boundingrects"));
    debugShapes_        = parser.isSet(QStringLiteral("debug-shapes"));
    debugHandles_       = parser.isSet(QStringLiteral("debug-handles"));
}

void CommandlineArgs::parse(const QStringList& args)
{
    QCommandLineParser parser;
    addOptions(parser);
    parser.parse(args);  // does not exit on unknown options

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        filename_ = positional.first();

    if (parser.isSet(QStringLiteral("settings-dir")))
        settingsDir_ = parser.value(QStringLiteral("settings-dir"));
    if (parser.isSet(QStringLiteral("loglevel")))
        loglevel_ = parser.value(QStringLiteral("loglevel"));
    debugBoundingRects_ = parser.isSet(QStringLiteral("debug-boundingrects"));
    debugShapes_        = parser.isSet(QStringLiteral("debug-shapes"));
    debugHandles_       = parser.isSet(QStringLiteral("debug-handles"));
}

// ─── FamSettings ─────────────────────────────────────────────────────────────

// Called in the member initializer list so the path is set before QSettings
// reads its file location. Mirrors Python's QSettings.setPath() call.
QSettings::Format FamSettings::initPathAndReturnFormat()
{
    const QString dir = CommandlineArgs::instance().settingsDir();
    if (!dir.isEmpty())
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir);
    return QSettings::IniFormat;
}

FamSettings::FamSettings()
    : QSettings(initPathAndReturnFormat(),
                QSettings::UserScope,
                qApp->organizationName(),
                qApp->applicationName())
{}

void FamSettings::updateRecentFiles(const QString& filename)
{
    const QString abs = QFileInfo(filename).absoluteFilePath();

    QStringList values = getRecentFiles();
    values.removeAll(abs);
    values.prepend(abs);
    if (values.size() > 10)
        values = values.mid(0, 10);

    beginWriteArray(QStringLiteral("RecentFiles"));
    for (int i = 0; i < values.size(); ++i) {
        setArrayIndex(i);
        setValue(QStringLiteral("path"), values[i]);
    }
    endArray();
}

QStringList FamSettings::getRecentFiles(bool existingOnly) const
{
    QStringList values;
    // QSettings array API is non-const; cast away is safe since no mutation
    // of the stored data occurs — only internal read state changes.
    auto* s = const_cast<FamSettings*>(this);
    const int size = s->beginReadArray(QStringLiteral("RecentFiles"));
    for (int i = 0; i < size; ++i) {
        s->setArrayIndex(i);
        values.append(value(QStringLiteral("path")).toString());
    }
    s->endArray();

    if (existingOnly) {
        values.erase(
            std::remove_if(values.begin(), values.end(),
                           [](const QString& f) { return !QFileInfo::exists(f); }),
            values.end());
    }
    return values;
}

// ─── KeyboardSettings ─────────────────────────────────────────────────────────

KeyboardSettings::KeyboardSettings()
    : QSettings(QFileInfo(FamSettings().fileName()).dir().filePath(
                    QStringLiteral("KeyboardSettings.ini")),
                QSettings::IniFormat)
{}

void KeyboardSettings::setShortcuts(const QString& group, const QString& key,
                                    const QStringList& values)
{
    setValue(group + QLatin1Char('/') + key, values.join(QStringLiteral(", ")));
}

QStringList KeyboardSettings::getShortcuts(const QString& group, const QString& key,
                                           const QStringList& defaultValues)
{
    const QVariant v = value(group + QLatin1Char('/') + key);
    if (v.isValid()) {
        QStringList values;
        for (const QString& s : v.toString().split(QStringLiteral(", "))) {
            if (!s.isEmpty())
                values.append(s);
        }
        return values;
    }

    if (saveUnknownShortcuts)
        setShortcuts(group, key, defaultValues);
    return defaultValues;
}

// ─── logfileName ─────────────────────────────────────────────────────────────

QString logfileName()
{
    return QFileInfo(FamSettings().fileName()).dir().filePath(
        qApp->applicationName() + QStringLiteral(".log"));
}

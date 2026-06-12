#include "settings.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
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

// ─── SettingsEvents ───────────────────────────────────────────────────────────

SettingsEvents& SettingsEvents::instance()
{
    static SettingsEvents inst;
    return inst;
}

// ─── FamSettings ─────────────────────────────────────────────────────────────

const QMap<QString, FieldConfig>& FamSettings::fields()
{
    static const QMap<QString, FieldConfig> map = {
        {
            "Save/confirm_close_unsaved",
            {
                /*default*/ true,
                /*cast*/    [](const QVariant& v) -> QVariant { return v.toBool(); },
            }
        },
        {
            "Items/image_storage_format",
            {
                /*default*/  QString("best"),
                /*cast*/     {},
                /*validate*/ [](const QVariant& v) {
                    const QString s = v.toString();
                    return s == QLatin1String("png")
                        || s == QLatin1String("jpg")
                        || s == QLatin1String("best");
                },
            }
        },
        {
            "Items/arrange_gap",
            {
                /*default*/  0,
                /*cast*/     [](const QVariant& v) -> QVariant { return v.toInt(); },
                /*validate*/ [](const QVariant& v) {
                    const int n = v.toInt();
                    return n >= 0 && n <= 200;
                },
            }
        },
        {
            "Items/arrange_default",
            {
                /*default*/  QString("optimal"),
                /*cast*/     {},
                /*validate*/ [](const QVariant& v) {
                    const QString s = v.toString();
                    return s == QLatin1String("optimal")
                        || s == QLatin1String("horizontal")
                        || s == QLatin1String("vertical")
                        || s == QLatin1String("square");
                },
            }
        },
        {
            "Items/image_allocation_limit",
            {
                /*default*/          256,
                /*cast*/             [](const QVariant& v) -> QVariant { return v.toInt(); },
                /*validate*/         [](const QVariant& v) { return v.toInt() >= 0; },
                /*postSaveCallback*/ [](const QVariant& v) {
                    QImageReader::setAllocationLimit(v.toInt());
                },
            }
        },
    };
    return map;
}

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

QVariant FamSettings::valueOrDefault(const QString& key) const
{
    const auto& f = fields();
    Q_ASSERT(f.contains(key));
    const FieldConfig& conf = f[key];

    QVariant val = QSettings::value(key);
    if (!val.isValid())
        return conf.defaultValue;

    if (conf.cast) {
        try {
            val = conf.cast(val);
        } catch (...) {
            return conf.defaultValue;
        }
    }
    if (conf.validate && !conf.validate(val))
        return conf.defaultValue;

    return val;
}

bool FamSettings::valueChanged(const QString& key) const
{
    return valueOrDefault(key) != fields().value(key).defaultValue;
}

void FamSettings::restoreDefaults()
{
    for (const QString& key : fields().keys())
        remove(key);
    emit SettingsEvents::instance().restoreDefaults();
}

void FamSettings::onStartup()
{
    const QByteArray envAlloc = qgetenv("QT_IMAGEIO_MAXALLOC");
    if (!envAlloc.isEmpty()) {
        QImageReader::setAllocationLimit(envAlloc.toInt());
    } else {
        const int alloc = valueOrDefault(QStringLiteral("Items/image_allocation_limit")).toInt();
        QImageReader::setAllocationLimit(alloc);
    }
}

void FamSettings::setValue(const QString& key, const QVariant& value)
{
    QSettings::setValue(key, value);
    const auto& f = fields();
    if (f.contains(key) && f[key].postSaveCallback)
        f[key].postSaveCallback(value);
}

void FamSettings::remove(const QString& key)
{
    QSettings::remove(key);
    const auto& f = fields();
    if (f.contains(key) && f[key].postSaveCallback)
        f[key].postSaveCallback(valueOrDefault(key));
}

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
        QSettings::setValue(QStringLiteral("path"), values[i]);
    }
    endArray();
}

QStringList FamSettings::getRecentFiles(bool existingOnly) const
{
    QStringList values;
    auto* s = const_cast<FamSettings*>(this);
    const int size = s->beginReadArray(QStringLiteral("RecentFiles"));
    for (int i = 0; i < size; ++i) {
        s->setArrayIndex(i);
        values.append(QSettings::value(QStringLiteral("path")).toString());
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

// ─── logfileName ─────────────────────────────────────────────────────────────

QString logfileName()
{
    return QFileInfo(FamSettings().fileName()).dir().filePath(
        qApp->applicationName() + QStringLiteral(".log"));
}

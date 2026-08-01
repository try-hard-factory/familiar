#include "settings.h"
#include "settingshandler.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

// ─── CommandlineArgs ──────────────────────────────────────────────────────────

CommandlineArgs& CommandlineArgs::instance()
{
    static CommandlineArgs inst;
    return inst;
}

static void addOptions(QCommandLineParser& parser)
{
    parser.addPositionalArgument(QStringLiteral("filename"),
                                 QCoreApplication::tr(
                                     "Familiar project file to open"),
                                 QStringLiteral("[filename]"));

    parser.addOption(
        {QStringList{QStringLiteral("f"), QStringLiteral("file")},
         QCoreApplication::tr("Familiar project file to open "
                              "(overrides the positional filename argument)"),
         QStringLiteral("path")});

    parser.addOption(
        {QStringLiteral("settings"),
         QCoreApplication::tr(
             "JSON settings file to use instead of the default location"),
         QStringLiteral("path")});

    parser.addOption(
        {QStringList{QStringLiteral("l"), QStringLiteral("loglevel")},
         QCoreApplication::tr("Log level for console output"),
         QStringLiteral("level"),
         QStringLiteral("INFO")});

    parser.addOption(
        {QStringLiteral("debug-boundingrects"),
         QCoreApplication::tr("Draw item's bounding rects for debugging")});

    parser.addOption(
        {QStringLiteral("debug-shapes"),
         QCoreApplication::tr("Draw item's mouse event shapes for debugging")});

    parser.addOption({QStringLiteral("debug-handles"),
                      QCoreApplication::tr(
                          "Draw item's transform handle areas for debugging")});
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
    if (parser.isSet(QStringLiteral("file")))
        filename_ = parser.value(QStringLiteral("file"));

    settingsFile_ = parser.value(QStringLiteral("settings"));
    loglevel_ = parser.value(QStringLiteral("loglevel"));
    debugBoundingRects_ = parser.isSet(QStringLiteral("debug-boundingrects"));
    debugShapes_ = parser.isSet(QStringLiteral("debug-shapes"));
    debugHandles_ = parser.isSet(QStringLiteral("debug-handles"));
}

void CommandlineArgs::parse(const QStringList& args)
{
    QCommandLineParser parser;
    addOptions(parser);
    parser.parse(args); // does not exit on unknown options

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        filename_ = positional.first();
    if (parser.isSet(QStringLiteral("file")))
        filename_ = parser.value(QStringLiteral("file"));

    if (parser.isSet(QStringLiteral("settings")))
        settingsFile_ = parser.value(QStringLiteral("settings"));
    if (parser.isSet(QStringLiteral("loglevel")))
        loglevel_ = parser.value(QStringLiteral("loglevel"));
    debugBoundingRects_ = parser.isSet(QStringLiteral("debug-boundingrects"));
    debugShapes_ = parser.isSet(QStringLiteral("debug-shapes"));
    debugHandles_ = parser.isSet(QStringLiteral("debug-handles"));
}

// ─── SettingsEvents ───────────────────────────────────────────────────────────

SettingsEvents& SettingsEvents::instance()
{
    static SettingsEvents inst;
    return inst;
}

// ─── FamSettings ─────────────────────────────────────────────────────────────

namespace {

// "Save/confirm_close_unsaved" -> group="Save", subkey="confirm_close_unsaved".
QString keyGroup(const QString& key)
{
    return key.section(QLatin1Char('/'), 0, 0);
}

QString keySubkey(const QString& key)
{
    return key.section(QLatin1Char('/'), 1);
}

} // namespace

const QMap<QString, FieldConfig>& FamSettings::fields()
{
    static const QMap<QString, FieldConfig> map = {
        {"Save/confirm_close_unsaved",
         {
             /*default*/ true,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toBool(); },
         }},
        {"Items/image_storage_format",
         {
             /*default*/ QString("best"),
             /*cast*/ {},
             /*validate*/
             [](const QVariant& v) {
                 const QString s = v.toString();
                 return s == QLatin1String("png") || s == QLatin1String("jpg")
                        || s == QLatin1String("best");
             },
         }},
        {"Items/arrange_gap",
         {
             /*default*/ 0,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/
             [](const QVariant& v) {
                 const int n = v.toInt();
                 return n >= 0 && n <= 200;
             },
         }},
        {"Items/arrange_default",
         {
             /*default*/ QString("optimal"),
             /*cast*/ {},
             /*validate*/
             [](const QVariant& v) {
                 const QString s = v.toString();
                 return s == QLatin1String("optimal")
                        || s == QLatin1String("horizontal")
                        || s == QLatin1String("vertical")
                        || s == QLatin1String("square");
             },
         }},
        {"Items/image_allocation_limit",
         {
             /*default*/ 256,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/ [](const QVariant& v) { return v.toInt() >= 0; },
             /*postSaveCallback*/
             [](const QVariant& v) {
                 QImageReader::setAllocationLimit(v.toInt());
             },
         }},
    };
    return map;
}

QVariant FamSettings::valueOrDefault(const QString& key) const
{
    const auto& f = fields();
    Q_ASSERT(f.contains(key));
    const FieldConfig& conf = f[key];

    const QJsonValue raw
        = SettingsHandler::getInstance()->jsonValue(keyGroup(key),
                                                    keySubkey(key));
    if (raw.isUndefined())
        return conf.defaultValue;
    QVariant val = raw.toVariant();

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
    SettingsHandler::getInstance()->removeJsonGroup(QStringLiteral("Save"));
    SettingsHandler::getInstance()->removeJsonGroup(QStringLiteral("Items"));
    for (const QString& key : fields().keys()) {
        const auto& conf = fields()[key];
        if (conf.postSaveCallback)
            conf.postSaveCallback(conf.defaultValue);
    }
    emit SettingsEvents::instance().restoreDefaults();
}

void FamSettings::onStartup()
{
    const QByteArray envAlloc = qgetenv("QT_IMAGEIO_MAXALLOC");
    if (!envAlloc.isEmpty()) {
        QImageReader::setAllocationLimit(envAlloc.toInt());
    } else {
        const int alloc = valueOrDefault(
                              QStringLiteral("Items/image_allocation_limit"))
                              .toInt();
        QImageReader::setAllocationLimit(alloc);
    }
}

void FamSettings::setValue(const QString& key, const QVariant& value)
{
    SettingsHandler::getInstance()->setJsonValue(keyGroup(key),
                                                 keySubkey(key),
                                                 QJsonValue::fromVariant(value));
    const auto& f = fields();
    if (f.contains(key) && f[key].postSaveCallback)
        f[key].postSaveCallback(value);
}

QVariant FamSettings::value(const QString& key,
                            const QVariant& defaultValue) const
{
    const QJsonValue raw
        = SettingsHandler::getInstance()->jsonValue(keyGroup(key),
                                                    keySubkey(key));
    return raw.isUndefined() ? defaultValue : raw.toVariant();
}

void FamSettings::remove(const QString& key)
{
    SettingsHandler::getInstance()->removeJsonValue(keyGroup(key),
                                                    keySubkey(key));
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

    SettingsHandler::getInstance()->setRecentFilesRaw(values);
}

QStringList FamSettings::getRecentFiles(bool existingOnly) const
{
    QStringList values = SettingsHandler::getInstance()->recentFilesRaw();

    if (existingOnly) {
        values.erase(std::remove_if(values.begin(),
                                    values.end(),
                                    [](const QString& f) {
                                        return !QFileInfo::exists(f);
                                    }),
                     values.end());
    }
    return values;
}

QString FamSettings::fileName() const
{
    return SettingsHandler::getInstance()->settingsFilePath();
}

// ─── logfileName ─────────────────────────────────────────────────────────────

QString logfileName()
{
    return QFileInfo(FamSettings().fileName())
        .dir()
        .filePath(qApp->applicationName() + QStringLiteral(".log"));
}

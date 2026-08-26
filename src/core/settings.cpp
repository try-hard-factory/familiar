#include "settings.h"
#include "settingshandler.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

#include <cstdio>
#include <cstdlib>

// ─── CommandlineArgs ──────────────────────────────────────────────────────────

CommandlineArgs& CommandlineArgs::instance()
{
    static CommandlineArgs inst;
    return inst;
}

namespace {

// A bare "-" is the getopt/QCommandLineParser convention for "not an
// option" (other CLI tools often read it as stdin) - familiar has no
// stdin project format, so it's not a real path either. "?" doesn't
// start with '-' at all, so the parser can't flag it as a bad option on
// its own - it looks exactly like any other (if wrong) filename to
// QCommandLineParser. Both silently fell through to filename_ before,
// only failing much later with a generic "file not found" from the
// file-opening code - this catches the same "clearly not meant as a
// path" case up front instead, with a proper CLI-style error.
bool looksLikeAPlaceholderNotAPath(const QString& arg)
{
    const QString trimmed = arg.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    for (const QChar c : trimmed) {
        if (c != QLatin1Char('-') && c != QLatin1Char('?')) {
            return false;
        }
    }
    return true;
}

} // namespace

static void addOptions(QCommandLineParser& parser)
{
    parser.addOption(
        {QStringList{QStringLiteral("f"), QStringLiteral("file")},
         QCoreApplication::tr("Familiar project file to open "
                              "(takes priority over a bare path argument)"),
         QStringLiteral("path")});

    parser.addOption(
        {QStringLiteral("settings"),
         QCoreApplication::tr(
             "JSON settings file to use instead of the default location"),
         QStringLiteral("path")});

    parser.addOption(
        {QStringList{QStringLiteral("l"), QStringLiteral("loglevel")},
         QCoreApplication::tr("Log level for console output (default: INFO)"),
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
    parser.setApplicationDescription(QCoreApplication::tr(
        "A canvas for collecting, arranging, and annotating reference "
        "images."));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    addOptions(parser);

    // parser.parse() + manual handling below, not parser.process(app) -
    // process() exits on error with Qt's own terse one-liner ("familiar:
    // Unknown option 'x'.") and no help text at all, leaving the user to
    // go look up the actual options themselves. This is the pattern
    // Qt's own docs recommend for a customized error path
    // (https://doc.qt.io/qt-6/qcommandlineparser.html's "complex
    // applications" example: parse()+errorText(), not process()) - an
    // unknown/malformed option now prints the same helpText() as -h,
    // right under the specific error message. Long options already
    // accept `--option=value` as well as `--option value` - that's
    // QCommandLineParser's own native behavior, nothing to add for it.
    if (!parser.parse(app.arguments())) {
        std::fputs(qPrintable(parser.errorText()), stderr);
        std::fputs("\n\n", stderr);
        std::fputs(qPrintable(parser.helpText()), stderr);
        std::exit(EXIT_FAILURE);
    }
    if (parser.isSet(versionOption)) {
        std::printf("%s %s\n",
                    qPrintable(QCoreApplication::applicationName()),
                    qPrintable(QCoreApplication::applicationVersion()));
        std::exit(EXIT_SUCCESS);
    }
    if (parser.isSet(helpOption)) {
        std::fputs(qPrintable(parser.helpText()), stdout);
        std::exit(EXIT_SUCCESS);
    }

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        if (looksLikeAPlaceholderNotAPath(positional.first())) {
            const QString msg = QStringLiteral("%1: Not a valid file path: \"%2\".\n\n")
                                     .arg(QCoreApplication::applicationName(),
                                          positional.first());
            std::fputs(qPrintable(msg), stderr);
            std::fputs(qPrintable(parser.helpText()), stderr);
            std::exit(EXIT_FAILURE);
        }
        filename_ = positional.first();
    }
    if (parser.isSet(QStringLiteral("file"))) {
        filename_ = parser.value(QStringLiteral("file"));
    }

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
    if (!positional.isEmpty()) {
        filename_ = positional.first();
    }
    if (parser.isSet(QStringLiteral("file"))) {
        filename_ = parser.value(QStringLiteral("file"));
    }

    if (parser.isSet(QStringLiteral("settings"))) {
        settingsFile_ = parser.value(QStringLiteral("settings"));
    }
    if (parser.isSet(QStringLiteral("loglevel"))) {
        loglevel_ = parser.value(QStringLiteral("loglevel"));
    }
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
             // Matches the Maximum Image Size row's spinbox ceiling
             // (widgets/setting_row.cpp's MaximumImageSizeRow) - kept in
             // sync since valueOrDefault() results also feed
             // QImageReader::setAllocationLimit() directly at startup
             // (below), not just through that row's own UI clamp.
             /*default*/ 32,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/
             [](const QVariant& v) {
                 const int n = v.toInt();
                 return n >= 0 && n <= 32;
             },
             /*postSaveCallback*/
             [](const QVariant& v) {
                 QImageReader::setAllocationLimit(v.toInt());
             },
         }},
        {"Items/undo_history_size",
         {
             // Matches the hardcoded undoStack_->setUndoLimit(100) this
             // is meant to replace (canvasview.cpp) - not wired up to it
             // yet, UI only for now.
             /*default*/ 100,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/ [](const QVariant& v) { return v.toInt() >= 0; },
         }},
        {"Items/auto_optimize_imported_images",
         {
             /*default*/ QString("warn"),
             /*cast*/ {},
             /*validate*/
             [](const QVariant& v) {
                 const QString s = v.toString();
                 return s == QLatin1String("off") || s == QLatin1String("warn")
                        || s == QLatin1String("optimize_large");
             },
         }},
        {"Save/autosave_enabled",
         {
             /*default*/ false,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toBool(); },
             /*validate*/ {},
             /*postSaveCallback*/
             [](const QVariant&) {
                 emit SettingsEvents::instance().autosaveSettingsChanged();
             },
         }},
        {"Save/autosave_interval_seconds",
         {
             /*default*/ 5,
             /*cast*/ [](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/
             [](const QVariant& v) {
                 const int n = v.toInt();
                 return n >= 1 && n <= 3600;
             },
             /*postSaveCallback*/
             [](const QVariant&) {
                 emit SettingsEvents::instance().autosaveSettingsChanged();
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
    if (raw.isUndefined()) {
        return conf.defaultValue;
    }
    QVariant val = raw.toVariant();

    if (conf.cast) {
        try {
            val = conf.cast(val);
        } catch (...) {
            return conf.defaultValue;
        }
    }
    if (conf.validate && !conf.validate(val)) {
        return conf.defaultValue;
    }

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
        if (conf.postSaveCallback) {
            conf.postSaveCallback(conf.defaultValue);
        }
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
    if (f.contains(key) && f[key].postSaveCallback) {
        f[key].postSaveCallback(value);
    }
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
    if (f.contains(key) && f[key].postSaveCallback) {
        f[key].postSaveCallback(valueOrDefault(key));
    }
}

void FamSettings::updateRecentFiles(const QString& filename)
{
    const QString abs = QFileInfo(filename).absoluteFilePath();

    QStringList values = getRecentFiles();
    values.removeAll(abs);
    values.prepend(abs);
    if (values.size() > 10) {
        values = values.mid(0, 10);
    }

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

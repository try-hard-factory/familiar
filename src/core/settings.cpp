#include "settings.h"
#include "settingshandler.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "log/log.h"
using namespace familiar::log;

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
bool looks_like_a_placeholder_not_a_path(const QString& arg)
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

void add_options(QCommandLineParser& parser)
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

} // namespace

void CommandlineArgs::process(const QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::tr(
        "A canvas for collecting, arranging, and annotating reference "
        "images."));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    add_options(parser);

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
    // Through `app`, not QCoreApplication::arguments(): the parameter is
    // what makes "a QCoreApplication must already exist" part of this
    // function's signature instead of an unwritten precondition.
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
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
        if (looks_like_a_placeholder_not_a_path(positional.first())) {
            const QString msg = QStringLiteral(
                                    "%1: Not a valid file path: \"%2\".\n\n")
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
    add_options(parser);
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
QString key_group(const QString& key)
{
    return key.section(QLatin1Char('/'), 0, 0);
}

QString key_subkey(const QString& key)
{
    return key.section(QLatin1Char('/'), 1);
}

} // namespace

const QMap<QString, FieldConfig>& FamSettings::fields()
{
    static const QMap<QString, FieldConfig> map = {
        {"Items/image_storage_format",
         {
             /*default*/ .defaultValue = QString("best"),
             /*cast*/ .cast = {},
             /*validate*/
             .validate =
                 [](const QVariant& v) {
                     const QString s = v.toString();
                     return s == QLatin1String("png")
                            || s == QLatin1String("jpg")
                            || s == QLatin1String("best");
                 },
             /*postSaveCallback*/
             .postSaveCallback = []([[maybe_unused]] const QVariant& v) {},
         }},
        {"Items/arrange_gap",
         {
             /*default*/ .defaultValue=0,
             /*cast*/ .cast=[](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/
             .validate=[](const QVariant& v) {
                 const int n = v.toInt();
                 return n >= 0 && n <= 200;
             },
             /*postSaveCallback*/
             .postSaveCallback=[]([[maybe_unused]] const QVariant& v) {},
         }},
        {"Items/arrange_default",
         {
             /*default*/ .defaultValue=QString("optimal"),
             /*cast*/ .cast={},
             /*validate*/
             .validate=[](const QVariant& v) {
                 const QString s = v.toString();
                 return s == QLatin1String("optimal")
                        || s == QLatin1String("horizontal")
                        || s == QLatin1String("vertical")
                        || s == QLatin1String("square");
             },
             /*postSaveCallback*/
             .postSaveCallback=[]([[maybe_unused]] const QVariant& v) {},
         }},
        {"Items/image_allocation_limit",
         {
             // Ceiling here MUST match the Maximum Image Size row's own
             // spinbox ceiling (widgets/setting_row.cpp's
             // MaximumImageSizeRow, currently 1024) - valueOrDefault()
             // results feed QImageReader::setAllocationLimit() directly
             // at startup (below), not just through that row's own UI
             // clamp, so a mismatch here silently rejects and reverts
             // anything the UI itself let the user actually type in. Real
             // bug this fixes: this was hardcoded to 32 while the
             // spinbox's own ceiling was already 1024 - any value above
             // 32 typed into that row saved fine but then silently
             // reverted back to the default (32) on the very next read,
             // with no indication why.
             // Was 32 - too tight for an ordinary modern camera/phone
             // photo (20+ MP commonly decodes past 32MB at 32 bits/pixel,
             // e.g. a real 6240x3512 photo ≈ 83.6MB - a genuine bug
             // report this raised).
             /*default*/ .defaultValue=256,
             /*cast*/ .cast=[](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/
             .validate=[](const QVariant& v) {
                 const int n = v.toInt();
                 return n >= 0 && n <= 1024;
             },
             /*postSaveCallback*/
             .postSaveCallback=[](const QVariant& v) {
                 QImageReader::setAllocationLimit(v.toInt());
             },
         }},
        {"Items/undo_history_size",
         {
             // Matches the hardcoded undoStack_->setUndoLimit(100) this
             // is meant to replace (canvasview.cpp) - not wired up to it
             // yet, UI only for now.
             /*default*/ .defaultValue=100,
             /*cast*/ .cast=[](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/ .validate=[](const QVariant& v) { return v.toInt() >= 0; },
             /*postSaveCallback*/
             .postSaveCallback=[]([[maybe_unused]] const QVariant& v) {},
         }},
        {"Items/auto_optimize_imported_images",
         {
             /*default*/ .defaultValue=QString("warn"),
             /*cast*/ .cast={},
             /*validate*/
             .validate=[](const QVariant& v) {
                 const QString s = v.toString();
                 return s == QLatin1String("off") || s == QLatin1String("warn")
                        || s == QLatin1String("optimize_large");
             },
             /*postSaveCallback*/
             .postSaveCallback=[]([[maybe_unused]] const QVariant& v) {},
         }},
        {"Items/raw_import_choice",
         {
             // "ask" (default) shows RawImportDialog per RAW file (or
             // per queue - see "Apply choice to this queue" there,
             // which is a transient in-batch decision, not this
             // setting). The other two skip that dialog entirely from
             // then on - set by that dialog's own "Remember choice for
             // future files" checkbox (widgets/raw_import_dialog.cpp),
             // not exposed as its own row on the Performance page.
             /*default*/ .defaultValue=QString("ask"),
             /*cast*/ .cast={},
             /*validate*/
             .validate=[](const QVariant& v) {
                 const QString s = v.toString();
                 return s == QLatin1String("ask")
                        || s == QLatin1String("always_optimize")
                        || s == QLatin1String("always_keep_original");
             },
             /*postSaveCallback*/
             .postSaveCallback=[]([[maybe_unused]] const QVariant& v) {},
         }},
        {"Save/autosave_enabled",
         {
             /*default*/ .defaultValue = false,
             /*cast*/ .cast = [](const QVariant& v) -> QVariant {
                 return v.toBool();
             },
             /*validate*/ .validate = {},
             /*postSaveCallback*/
             .postSaveCallback =
                 [](const QVariant&) {
                     emit SettingsEvents::instance().autosave_settings_changed();
                 },
         }},
        {"Save/autosave_interval_seconds",
         {
             /*default*/ .defaultValue=5,
             /*cast*/ .cast=[](const QVariant& v) -> QVariant { return v.toInt(); },
             /*validate*/
             .validate=[](const QVariant& v) {
                 const int n = v.toInt();
                 return n >= 1 && n <= 3600;
             },
             /*postSaveCallback*/
             .postSaveCallback=[](const QVariant&) {
                 emit SettingsEvents::instance().autosave_settings_changed();
             },
         }},
    };
    return map;
}

QVariant FamSettings::value_or_default(const QString& key)
{
    const auto& f = fields();
    Q_ASSERT(f.contains(key));
    const FieldConfig& conf = f[key];

    const QJsonValue raw
        = SettingsHandler::get_instance()->json_value(key_group(key),
                                                    key_subkey(key));
    if (raw.isUndefined()) {
        return conf.defaultValue;
    }
    QVariant val = raw.toVariant();

    if (conf.cast) {
        try {
            val = conf.cast(val);
        } catch (...) {
            FLOG_WARN(Ch::Settings,
                      "{}: stored value {} threw during cast, falling back "
                      "to default",
                      key,
                      val.toString());
            return conf.defaultValue;
        }
    }
    if (conf.validate && !conf.validate(val)) {
        FLOG_WARN(Ch::Settings,
                  "{}: stored value {} failed validation, falling back to "
                  "default",
                  key,
                  val.toString());
        return conf.defaultValue;
    }

    return val;
}

bool FamSettings::value_changed(const QString& key)
{
    return value_or_default(key) != fields().value(key).defaultValue;
}

void FamSettings::restore_defaults()
{
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Save"));
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Items"));
    for (const QString& key : fields().keys()) {
        const auto& conf = fields()[key];
        if (conf.postSaveCallback) {
            conf.postSaveCallback(conf.defaultValue);
        }
    }
    emit SettingsEvents::instance().restore_defaults();
}

void FamSettings::on_startup()
{
    const QByteArray envAlloc = qgetenv("QT_IMAGEIO_MAXALLOC");
    if (!envAlloc.isEmpty()) {
        QImageReader::setAllocationLimit(envAlloc.toInt());
    } else {
        const int alloc = value_or_default(
                              QStringLiteral("Items/image_allocation_limit"))
                              .toInt();
        QImageReader::setAllocationLimit(alloc);
    }
}

void FamSettings::set_value(const QString& key, const QVariant& value)
{
    SettingsHandler::get_instance()->set_json_value(key_group(key),
                                                 key_subkey(key),
                                                 QJsonValue::fromVariant(value));
    const auto& f = fields();
    if (f.contains(key) && f[key].postSaveCallback) {
        f[key].postSaveCallback(value);
    }
}

QVariant FamSettings::value(const QString& key,
                            const QVariant& defaultValue)
{
    const QJsonValue raw
        = SettingsHandler::get_instance()->json_value(key_group(key),
                                                    key_subkey(key));
    return raw.isUndefined() ? defaultValue : raw.toVariant();
}

void FamSettings::remove(const QString& key)
{
    SettingsHandler::get_instance()->remove_json_value(key_group(key),
                                                    key_subkey(key));
    const auto& f = fields();
    if (f.contains(key) && f[key].postSaveCallback) {
        f[key].postSaveCallback(value_or_default(key));
    }
}

void FamSettings::update_recent_files(const QString& filename)
{
    const QString abs = QFileInfo(filename).absoluteFilePath();

    QStringList values = get_recent_files();
    values.removeAll(abs);
    values.prepend(abs);
    if (values.size() > 10) {
        values = values.mid(0, 10);
    }

    SettingsHandler::get_instance()->set_recent_files_raw(values);
}

QStringList FamSettings::get_recent_files(bool existingOnly)
{
    QStringList values = SettingsHandler::get_instance()->recent_files_raw();

    if (existingOnly) {
        // .begin() on the result: the ranges overload returns a subrange,
        // not an iterator, so it can't be handed to erase() directly the
        // way std::remove_if's return value could.
        values.erase(std::ranges::remove_if(values,
                                            [](const QString& f) {
                                                return !QFileInfo::exists(f);
                                            })
                         .begin(),
                     values.end());
    }
    return values;
}

QString FamSettings::file_name()
{
    return SettingsHandler::get_instance()->settings_file_path();
}

#include "settingshandler.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>

#include <core/valuehandler.h>

#include "Logger.h"

extern Logger logger;

#define OPTION(KEY, TYPE)                                                      \
    {                                                                          \
        QStringLiteral(KEY), QSharedPointer<ValueHandler>(new TYPE)            \
    }

#define SHORTCUT(NAME, DEFAULT_VALUE)                                          \
    {                                                                          \
        QStringLiteral(NAME), QSharedPointer<KeySequence>(new KeySequence(     \
                                QKeySequence(QLatin1String(DEFAULT_VALUE))))   \
    }

static QMap<class QString, QSharedPointer<ValueHandler>>
        recognizedGeneralOptions = {
//         KEY                            TYPE                 DEFAULT_VALUE
    OPTION("option0"                    ,Bool               ( true          )),
    OPTION("option0"         ,Bool               ( true          )),

};

static QMap<QString, QSharedPointer<KeySequence>> recognizedShortcuts = {
//           NAME                           DEFAULT_SHORTCUT
    SHORTCUT("TYPE_SAVE"                ,   "Ctrl+S"                ),
    SHORTCUT("TYPE_EXIT"                ,   "Ctrl+Q"                ),

};


SettingsHandler::SettingsHandler()
    : settings_(QSettings::IniFormat,
                   QSettings::UserScope,
                   qApp->organizationName(),
                   qApp->applicationName())
{
    static bool firstInitialization = true;
    if (firstInitialization) {
        // check for error every time the file changes
        settingsWatcher_.reset(new QFileSystemWatcher());
        ensureFileWatched();
        QObject::connect(settingsWatcher_.data(),
                         &QFileSystemWatcher::fileChanged,
                         [](const QString& fileName) {
                             emit getInstance()->fileChanged();

                             if (QFile(fileName).exists()) {
                                 settingsWatcher_->addPath(fileName);
                             }
                             if (skipNextErrorCheck_) {
                                 skipNextErrorCheck_ = false;
                                 return;
                             }
                             SettingsHandler().checkAndHandleError();
                             if (!QFile(fileName).exists()) {
                                 // File watcher stops watching a deleted file.
                                 // Next time the config is accessed, force it
                                 // to check for errors (and watch again).
                                 errorCheckPending_ = true;
                             }
                         });
    }
    firstInitialization = false;
}


SettingsHandler* SettingsHandler::getInstance()
{
    static SettingsHandler config;
    return &config;
}


bool SettingsHandler::setShortcut(const QString& actionName, const QString& shortcut)
{
    LOG_DEBUG(logger, shortcut);
    qDebug() << actionName;
    static QVector<QKeySequence> reservedShortcuts = {
        Qt::Key_Backspace,
        Qt::Key_Escape,
    };

    if (hasError()) {
        return false;
    }

    bool error = false;

    settings_.beginGroup(SETTINGS_GROUP_SHORTCUTS);
    if (shortcut.isEmpty()) {
        setValue(actionName, "");
    } else if (reservedShortcuts.contains(QKeySequence(shortcut))) {
        // do not allow to set reserved shortcuts
        error = true;
    } else {
        error = false;
        // Make no difference for Return and Enter keys
        QString newShortcut = KeySequence().value(shortcut).toString();
        for (auto& otherAction : settings_.allKeys()) {
            if (actionName == otherAction) {
                continue;
            }
            QString existingShortcut =
              KeySequence().value(settings_.value(otherAction)).toString();
            if (newShortcut == existingShortcut) {
                error = true;
                goto done;
            }
        }
        settings_.setValue(actionName, KeySequence().value(shortcut));
    }
done:
    settings_.endGroup();
    return !error;
}


QString SettingsHandler::shortcut(const QString& actionName)
{

}

void SettingsHandler::setValue(const QString &key, const QVariant &value)
{
    assertKeyRecognized(key);
    if (!hasError()) {
        // don't let the file watcher initiate another error check
        skipNextErrorCheck_ = true;
        auto val = valueHandler(key)->representation(value);
        settings_.setValue(key, val);
    }
}


QSet<QString> &SettingsHandler::recognizedShortcutNames()
{
    auto keys = recognizedShortcuts.keys();
    static QSet<QString> names = QSet<QString>(keys.begin(), keys.end());

    return names;
}


void SettingsHandler::checkAndHandleError() const
{
    if (!QFile(settings_.fileName()).exists()) {
        setErrorState(false);
    } else {
//        setErrorState(!checkForErrors());
    }

    ensureFileWatched();
}


bool SettingsHandler::hasError() const
{
    if (errorCheckPending_) {
        checkAndHandleError();
        errorCheckPending_ = false;
    }
    return hasError_;
}


QString SettingsHandler::errorMessage() const
{
    return tr(
      "The configuration contains an error. Open configuration to resolve.");
}


void SettingsHandler::ensureFileWatched() const
{
    QFile file(settings_.fileName());
    if (!file.exists()) {
        file.open(QFileDevice::WriteOnly);
        file.close();
    }
    if (settingsWatcher_ != nullptr && settingsWatcher_->files().isEmpty() &&
        qApp != nullptr // ensures that the organization name can be accessed
    ) {
        settingsWatcher_->addPath(settings_.fileName());
    }
}


void SettingsHandler::assertKeyRecognized(const QString &key) const
{
    bool recognized = isShortcut(key)
            ? recognizedShortcutNames().contains(baseName(key))
            : ::recognizedGeneralOptions.contains(key);
    if (!recognized) {
        setErrorState(true);
    }
}


bool SettingsHandler::isShortcut(const QString &key) const
{
    return settings_.group() == QStringLiteral(SETTINGS_GROUP_SHORTCUTS) ||
              key.startsWith(QStringLiteral(SETTINGS_GROUP_SHORTCUTS "/"));
}


QString SettingsHandler::baseName(QString key) const
{
    return QFileInfo(key).baseName();
}


void SettingsHandler::setErrorState(bool error) const
{
    bool hadError = hasError_;
    hasError_ = error;
    // Notify user every time m_hasError changes
    if (!hadError && hasError_) {
        QString msg = errorMessage();
        LOG_WARNING(logger, msg);
//        AbstractLogger::error() << msg;
        emit getInstance()->error();
    } else if (hadError && !hasError_) {
        auto msg = tr("You have successfully resolved the configuration error.");
        LOG_INFO(logger, msg);
//        AbstractLogger::info() << msg;
        emit getInstance()->errorResolved();
    }
}

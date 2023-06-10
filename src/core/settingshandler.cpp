#include "settingshandler.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QKeySequence>
#include <QMap>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QVector>

#include <core/valuehandler.h>

#include "Logger.h"

extern Logger logger;

#define OPTION(KEY, TYPE) \
    { \
        QStringLiteral(KEY), QSharedPointer<ValueHandler>(new TYPE) \
    }

#define SHORTCUT(NAME, DEFAULT_VALUE) \
    { \
        QStringLiteral(NAME), QSharedPointer<KeySequence>(new KeySequence( \
                                  QKeySequence(QLatin1String(DEFAULT_VALUE)))) \
    }

static QMap<class QString, QSharedPointer<ValueHandler>> recognizedGeneralOptions
    = {
        //         KEY                            TYPE                 DEFAULT_VALUE
        OPTION("option0", Bool(true)),
        OPTION("option1", Bool(true)),

};

static QMap<QString, QSharedPointer<KeySequence>> recognizedShortcuts = {
    //           NAME                           DEFAULT_SHORTCUT
    SHORTCUT("TYPE_NEW", "Ctrl+N"),
    SHORTCUT("TYPE_OPEN", "Ctrl+O"),
    SHORTCUT("TYPE_SAVE", "Ctrl+S"),
    SHORTCUT("TYPE_QUIT", "Ctrl+Q"),

};


SettingsHandler::SettingsHandler()
    : settings_(QSettings::IniFormat,
                QSettings::UserScope,
                qApp->organizationName(),
                qApp->applicationName())
{
    //    settings_.clear();
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
                             getInstance()->checkAndHandleError();
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


bool SettingsHandler::setShortcut(const QString& actionName,
                                  const QString& shortcut)
{
    LOG_DEBUG(logger, shortcut.toStdString());

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
            QString existingShortcut
                = KeySequence().value(settings_.value(otherAction)).toString();
            if (newShortcut == existingShortcut) {
                error = true;
                goto done;
            }
        }
        settings_.setValue(actionName, KeySequence().value(shortcut));
    }
done:
    settings_.endGroup();
    emit getInstance()->shortCutChanged(actionName);
    return !error;
}


QString SettingsHandler::shortcut(const QString& actionName)
{
    QString setting = SETTINGS_GROUP_SHORTCUTS "/" + actionName;
    QString shortcut = value(setting).toString();
    if (!settings_.contains(setting)) {
        // The action uses a shortcut that is a flameshot default
        // (not set explicitly by user)
        settings_.beginGroup(SETTINGS_GROUP_SHORTCUTS);
        for (auto& otherAction : settings_.allKeys()) {
            if (settings_.value(otherAction) == shortcut) {
                // We found an explicit shortcut - it will take precedence
                settings_.endGroup();
                return {};
            }
        }
        settings_.endGroup();
    }
    return shortcut;
}


void SettingsHandler::setValue(const QString& key, const QVariant& value)
{
    assertKeyRecognized(key);
    if (!hasError()) {
        // don't let the file watcher initiate another error check
        skipNextErrorCheck_ = true;
        auto val = valueHandler(key)->representation(value);
        settings_.setValue(key, val);
    }
}


QVariant SettingsHandler::value(const QString& key) const
{
    assertKeyRecognized(key);

    auto val = settings_.value(key);

    auto handler = valueHandler(key);

    // Check the value for semantic errors
    if (val.isValid() && !handler->check(val)) {
        setErrorState(true);
    }
    if (hasError_) {
        return handler->fallback();
    }

    return handler->value(val);
}


void SettingsHandler::remove(const QString& key)
{
    settings_.remove(key);
}


void SettingsHandler::resetValue(const QString& key)
{
    settings_.setValue(key, valueHandler(key)->fallback());
}


QSet<QString>& SettingsHandler::recognizedShortcutNames()
{
    auto keys = recognizedShortcuts.keys();
    static QSet<QString> names = QSet<QString>(keys.begin(), keys.end());

    return names;
}


QSet<QString> SettingsHandler::keysFromGroup(const QString& group) const
{
    QSet<QString> keys;
    for (const QString& key : settings_.allKeys()) {
        if (group == SETTINGS_GROUP_GENERAL && !key.contains('/')) {
            keys.insert(key);
        } else if (key.startsWith(group + "/")) {
            keys.insert(baseName(key));
        }
    }
    return keys;
}


bool SettingsHandler::checkForErrors() const
{
    return checkUnrecognizedSettings() & checkShortcutConflicts()
           & checkSemantics();
}


bool SettingsHandler::checkUnrecognizedSettings(QList<QString>* offenders) const
{
    // sort the config keys by group
    QSet<QString> shortcutKeys = keysFromGroup(SETTINGS_GROUP_SHORTCUTS),
                  recognizedShortcutKeys = recognizedShortcutNames();

    // subtract recognized keys
    shortcutKeys.subtract(recognizedShortcutKeys);

    // what is left are the unrecognized keys - hopefully empty
    bool ok = shortcutKeys.isEmpty();
    if (offenders != nullptr) {
        for (const QString& key : shortcutKeys) {
            if (offenders) {
                offenders->append(SETTINGS_GROUP_SHORTCUTS "/" + key);
            }
        }
    }
    return ok;
}


bool SettingsHandler::checkShortcutConflicts() const
{
    bool ok = true;
    settings_.beginGroup(SETTINGS_GROUP_SHORTCUTS);
    QStringList shortcuts = settings_.allKeys();
    QStringList reportedInLog;
    for (auto key1 = shortcuts.begin(); key1 != shortcuts.end(); ++key1) {
        for (auto key2 = key1 + 1; key2 != shortcuts.end(); ++key2) {
            // values stored in variables are useful when running debugger
            QString value1 = settings_.value(*key1).toString(),
                    value2 = settings_.value(*key2).toString();
            // The check will pass if:
            // - one shortcut is empty (the action doesn't use a shortcut)
            // - or one of the settings is not found in m_settings, i.e.
            //   user wants to use flameshot's default shortcut for the action
            // - or the shortcuts for both actions are different
            if (!(value1.isEmpty() || !settings_.contains(*key1)
                  || !settings_.contains(*key2) || value1 != value2)) {
                ok = false;
                break;
            }
        }
    }
    settings_.endGroup();
    return ok;
}


bool SettingsHandler::checkSemantics(QList<QString>* offenders) const
{
    QStringList allKeys = settings_.allKeys();
    bool ok = true;
    for (const QString& key : allKeys) {
        QVariant val = settings_.value(key);
        auto valueHandler = this->valueHandler(key);
        if (val.isValid() && !valueHandler->check(val)) {
            // Key does not pass the check
            ok = false;
            if (offenders == nullptr) {
                break;
            }
            if (offenders != nullptr) {
                offenders->append(key);
            }
        }
    }
    return ok;
}


void SettingsHandler::checkAndHandleError() const
{
    if (!QFile(settings_.fileName()).exists()) {
        setErrorState(false);
    } else {
        setErrorState(!checkForErrors());
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
    if (settingsWatcher_ != nullptr && settingsWatcher_->files().isEmpty()
        && qApp != nullptr // ensures that the organization name can be accessed
    ) {
        settingsWatcher_->addPath(settings_.fileName());
    }
}


void SettingsHandler::assertKeyRecognized(const QString& key) const
{
    bool recognized = isShortcut(key)
                          ? recognizedShortcutNames().contains(baseName(key))
                          : ::recognizedGeneralOptions.contains(key);
    if (!recognized) {
        setErrorState(true);
    }
}


bool SettingsHandler::isShortcut(const QString& key) const
{
    return settings_.group() == QStringLiteral(SETTINGS_GROUP_SHORTCUTS)
           || key.startsWith(QStringLiteral(SETTINGS_GROUP_SHORTCUTS "/"));
}


QString SettingsHandler::baseName(QString key) const
{
    return QFileInfo(key).baseName();
}


QSharedPointer<ValueHandler> SettingsHandler::valueHandler(
    const QString& key) const
{
    QSharedPointer<ValueHandler> handler;
    if (isShortcut(key)) {
        handler = recognizedShortcuts.value(baseName(key),
                                            QSharedPointer<KeySequence>(
                                                new KeySequence()));
    } else { // General group
        handler = ::recognizedGeneralOptions.value(key);
    }
    return handler;
}


void SettingsHandler::setErrorState(bool error) const
{
    bool hadError = hasError_;
    hasError_ = error;
    // Notify user every time m_hasError changes
    if (!hadError && hasError_) {
        QString msg = errorMessage();
        LOG_WARNING(logger, msg.toStdString());
        //        AbstractLogger::error() << msg;
        emit getInstance()->error();
    } else if (hadError && !hasError_) {
        auto msg = tr(
            "You have successfully resolved the configuration error.");
        LOG_INFO(logger, msg.toStdString());
        //        AbstractLogger::info() << msg;
        emit getInstance()->errorResolved();
    }
}


// STATIC MEMBER DEFINITIONS

bool SettingsHandler::hasError_ = false;
bool SettingsHandler::errorCheckPending_ = true;
bool SettingsHandler::skipNextErrorCheck_ = false;

QSharedPointer<QFileSystemWatcher> SettingsHandler::settingsWatcher_;

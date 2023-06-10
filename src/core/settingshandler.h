#ifndef SETTINGSHANDLER_H
#define SETTINGSHANDLER_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariant>
#include <QVector>

#define SETTINGS_GROUP_GENERAL "General"
#define SETTINGS_GROUP_SHORTCUTS "Shortcuts"

class QFileSystemWatcher;
class ValueHandler;
template<class T>
class QSharedPointer;
class QTextStream;
//class AbstractLogger;

#define SETTINGS_GETTER(KEY, TYPE) \
    TYPE KEY() \
    { \
        return value(QStringLiteral(#KEY)).value<TYPE>(); \
    }
#define SETTINGS_SETTER(FUNC, KEY, TYPE) \
    void FUNC(const TYPE& val) \
    { \
        QString key = QStringLiteral(#KEY); \
        /* Without this check, multiple `flameshot gui` instances running */ \
        /* simultaneously would cause an endless loop of fileWatcher calls */ \
        if (QVariant::fromValue(val) != value(key)) { \
            setValue(key, QVariant::fromValue(val)); \
        } \
    }
#define SETTINGS_GETTER_SETTER(GETFUNC, SETFUNC, TYPE) \
    SETTINGS_GETTER(GETFUNC, TYPE) \
    SETTINGS_SETTER(SETFUNC, GETFUNC, TYPE)


class SettingsHandler : public QObject
{
    Q_OBJECT

public:
    explicit SettingsHandler();
    static SettingsHandler* getInstance();

    // GENERIC GETTERS AND SETTERS
    bool setShortcut(const QString& actionName, const QString& shortcut);
    QString shortcut(const QString& actionName);
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key) const;
    void remove(const QString& key);
    void resetValue(const QString& key);

    // INFO
    static QSet<QString>& recognizedShortcutNames();
    QSet<QString> keysFromGroup(const QString& group) const;

    // errors catching
    bool checkForErrors() const;
    bool checkUnrecognizedSettings(QList<QString>* offenders = nullptr) const;
    bool checkShortcutConflicts() const;
    bool checkSemantics(QList<QString>* offenders = nullptr) const;
    void checkAndHandleError() const;
    void setErrorState(bool error) const;
    bool hasError() const;
    QString errorMessage() const;


signals:
    void fileChanged();
    void error();
    void errorResolved();
    void shortCutChanged(const QString& t);

private:
    void ensureFileWatched() const;
    QSharedPointer<ValueHandler> valueHandler(const QString& key) const;
    void assertKeyRecognized(const QString& key) const;
    bool isShortcut(const QString& key) const;
    QString baseName(QString key) const;

private:
    mutable QSettings settings_;

    static bool hasError_;
    static bool errorCheckPending_;
    static bool skipNextErrorCheck_;

    static QSharedPointer<QFileSystemWatcher> settingsWatcher_;
};

#endif // SETTINGSHANDLER_H

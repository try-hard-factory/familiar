#ifndef SETTINGSHANDLER_H
#define SETTINGSHANDLER_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariant>
#include <QVector>

#define SETTINGS_GROUP_GENERAL "General"
#define SETTINGS_GROUP_SHORTCUTS "Shortcuts"

enum EPresets {
    kDarkPreset = 0,
    kLightPreset = 1,
    kCustom1 = 2,
    kCustom2 = 3,
    kCustom3 = 4,
    kCustom4 = 5,
    kAllPresets = 6
};

enum EPresetsColorIdx {
    kBackgroundColor = 0,
    kCanvasColor = 1,
    kBorderColor = 2,
    kTextColor = 3,
    kSelectionColor = 4,
    kAllIdx = 5
};

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
    SETTINGS_GETTER_SETTER(currentPreset, setCurrentPreset, int)
    using OL = QMap<int, int>;
    SETTINGS_GETTER_SETTER(masterOpacity, setMasterOpacity, OL)
    using CL = QMap<int, QColor>;
    SETTINGS_GETTER_SETTER(darkColorPreset, setDarkColorPreset, CL )
    SETTINGS_GETTER_SETTER(lightColorPreset, setLightColorPreset, CL )
    SETTINGS_GETTER_SETTER(customPreset1, setCustomPreset1, CL )
    SETTINGS_GETTER_SETTER(customPreset2, setCustomPreset2, CL )
    SETTINGS_GETTER_SETTER(customPreset3, setCustomPreset3, CL )
    SETTINGS_GETTER_SETTER(customPreset4, setCustomPreset4, CL )

    void setDefaultCurrentPreset();

    bool setShortcut(const QString& actionName, const QString& shortcut);
    QString shortcut(const QString& actionName);
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key) const;
    void remove(const QString& key);
    void resetValue(const QString& key);

    // INFO
    static QSet<QString>& recognizedGeneralOptions();
    static QSet<QString>& recognizedShortcutNames();
    QSet<QString> keysFromGroup(const QString& group) const;
    CL getCurrentColorPreset();
    void setCurrentColorPreset(const CL& preset);
    int getCurrentOpacity();
    void setCurrentOpacity(int opacity);

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
    void settingsChanged() const;
    void presetsChanged() const;
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

#include "controls.h"
#include "settings.h"
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QWheelEvent>
// TODO:
// ─── MouseConfigBase ──────────────────────────────────────────────────────────

MouseConfigBase::MouseConfigBase(const QString& id, const QString& group,
                                 const QString& text,
                                 const QStringList& defaultModifiers,
                                 bool invertible)
    : id_(id), group_(group), text_(text)
    , defaultModifiers_(defaultModifiers), invertible_(invertible)
{}

const QList<QPair<QString, Qt::KeyboardModifier>>& MouseConfigBase::modifierMap()
{
    static const QList<QPair<QString, Qt::KeyboardModifier>> map = {
        {"No Modifier", Qt::NoModifier},
        {"Shift",       Qt::ShiftModifier},
        {"Ctrl",        Qt::ControlModifier},
        {"Alt",         Qt::AltModifier},
        {"Meta",        Qt::MetaModifier},
        {"Keypad",      Qt::KeypadModifier},
    };
    return map;
}

const QList<QPair<QString, Qt::MouseButton>>& MouseConfigBase::buttonMap()
{
    static const QList<QPair<QString, Qt::MouseButton>> map = {
        {"Not Configured", Qt::NoButton},
        {"Left",           Qt::LeftButton},
        {"Middle",         Qt::MiddleButton},
    };
    return map;
}

Qt::KeyboardModifiers MouseConfigBase::modifiersToQt(const QStringList& modifiers)
{
    Qt::KeyboardModifiers result = Qt::NoModifier;
    const auto& map = modifierMap();
    for (const QString& name : modifiers) {
        for (const auto& [key, flag] : map) {
            if (key == name) {
                result |= flag;
                break;
            }
        }
    }
    return result;
}

QStringList MouseConfigBase::getModifiers() const
{
    return KeyboardSettings().getList(
        settingsGroup(), id_ + QStringLiteral("_modifiers"), defaultModifiers_);
}

void MouseConfigBase::setModifiers(const QStringList& values) const
{
    KeyboardSettings().setList(
        settingsGroup(), id_ + QStringLiteral("_modifiers"), values, defaultModifiers_);
}

bool MouseConfigBase::getInverted() const
{
    return KeyboardSettings()
        .getScalar(settingsGroup(), id_ + QStringLiteral("_inverted"), defaultInverted_)
        .toBool();
}

void MouseConfigBase::setInverted(bool value) const
{
    KeyboardSettings().setScalar(
        settingsGroup(), id_ + QStringLiteral("_inverted"), value, defaultInverted_);
}

// ─── MouseWheelConfig ─────────────────────────────────────────────────────────

MouseWheelConfig::MouseWheelConfig(const QString& id, const QString& group,
                                   const QString& text,
                                   const QStringList& defaultModifiers,
                                   bool invertible)
    : MouseConfigBase(id, group, text, defaultModifiers, invertible)
{}

const char* MouseWheelConfig::settingsGroup() const
{
    return "MouseWheel";
}

bool MouseWheelConfig::controlsChanged() const
{
    const QStringList cur = getModifiers();
    return QSet<QString>(cur.begin(), cur.end())
               != QSet<QString>(defaultModifiers_.begin(), defaultModifiers_.end())
           || getInverted() != defaultInverted_;
}

bool MouseWheelConfig::isConfigured() const
{
    return !getModifiers().isEmpty();
}

void MouseWheelConfig::removeControls() const
{
    setModifiers({});
    setInverted(false);
}

bool MouseWheelConfig::conflictsWith(const MouseWheelConfig& other) const
{
    if (!isConfigured() || !other.isConfigured())
        return false;
    const QStringList a = getModifiers(), b = other.getModifiers();
    return QSet<QString>(a.begin(), a.end()) == QSet<QString>(b.begin(), b.end());
}

bool MouseWheelConfig::matchesEvent(const QWheelEvent* event) const
{
    if (!isConfigured())
        return false;
    return modifiersToQt(getModifiers()) == event->modifiers();
}

// ─── MouseConfig ──────────────────────────────────────────────────────────────

MouseConfig::MouseConfig(const QString& id, const QString& group, const QString& text,
                         const QString& defaultButton,
                         const QStringList& defaultModifiers, bool invertible)
    : MouseConfigBase(id, group, text, defaultModifiers, invertible)
    , defaultButton_(defaultButton)
{}

const char* MouseConfig::settingsGroup() const
{
    return "Mouse";
}

QString MouseConfig::getButton() const
{
    return KeyboardSettings()
        .getScalar(settingsGroup(), id_ + QStringLiteral("_button"), defaultButton_)
        .toString();
}

void MouseConfig::setButton(const QString& value) const
{
    KeyboardSettings().setScalar(
        settingsGroup(), id_ + QStringLiteral("_button"), value, defaultButton_);
}

bool MouseConfig::controlsChanged() const
{
    const QStringList cur = getModifiers();
    return getButton() != defaultButton_
           || QSet<QString>(cur.begin(), cur.end())
                  != QSet<QString>(defaultModifiers_.begin(), defaultModifiers_.end())
           || getInverted() != defaultInverted_;
}

bool MouseConfig::isConfigured() const
{
    return getButton() != QLatin1String("Not Configured");
}

void MouseConfig::removeControls() const
{
    setButton(QStringLiteral("Not Configured"));
    setModifiers({});
    setInverted(false);
}

bool MouseConfig::conflictsWith(const MouseConfig& other) const
{
    if (!isConfigured() || !other.isConfigured())
        return false;
    const QStringList a = getModifiers(), b = other.getModifiers();
    return getButton() == other.getButton()
           && QSet<QString>(a.begin(), a.end()) == QSet<QString>(b.begin(), b.end());
}

bool MouseConfig::matchesEvent(const QMouseEvent* event) const
{
    if (!isConfigured())
        return false;
    const auto& bmap = buttonMap();
    Qt::MouseButton btn = Qt::NoButton;
    const QString bname = getButton();
    for (const auto& [key, flag] : bmap) {
        if (key == bname) { btn = flag; break; }
    }
    return modifiersToQt(getModifiers()) == event->modifiers()
           && btn == event->button();
}

// ─── KeyboardSettings ─────────────────────────────────────────────────────────

KeyboardSettings::KeyboardSettings()
    : QSettings(QFileInfo(FamSettings().fileName()).dir().filePath(
                    QStringLiteral("KeyboardSettings.ini")),
                QSettings::IniFormat)
{}

const QList<MouseWheelConfig>& KeyboardSettings::mousewheelActions()
{
    static const QList<MouseWheelConfig> list = {
        {"zoom1",          "zoom",           "Zoom",
         {"No Modifier"},                                   true},
        {"zoom2",          "zoom",           "Zoom (alternative)",
         {},                                                true},
        {"pan_horizontal1","pan_horizontal", "Pan horizontally",
         {"Shift"},                                         true},
        {"pan_horizontal2","pan_horizontal", "Pan horizontally (alternative)",
         {},                                                true},
        {"pan_vertical1",  "pan_vertical",   "Pan vertically",
         {"Shift", "Ctrl"},                                 true},
        {"pan_vertical2",  "pan_vertical",   "Pan vertically (alternative)",
         {},                                                true},
    };
    return list;
}

const QList<MouseConfig>& KeyboardSettings::mouseActions()
{
    static const QList<MouseConfig> list = {
        {"zoom1",       "zoom",        "Zoom",
         "Middle",      {"Ctrl"},            true},
        {"zoom2",       "zoom",        "Zoom (alternative)",
         "Not Configured", {},               true},
        {"pan1",        "pan",         "Pan",
         "Middle",      {"No Modifier"},     false},
        {"pan2",        "pan",         "Pan (alternative)",
         "Left",        {"Alt"},             false},
        {"movewindow1", "movewindow",  "Move Window",
         "Left",        {"Ctrl", "Alt"},     false},
        {"movewindow2", "movewindow",  "Move Window (alternative)",
         "Not Configured", {},               false},
    };
    return list;
}

void KeyboardSettings::setShortcuts(const QString& group, const QString& key,
                                    const QStringList& values)
{
    setValue(group + QLatin1Char('/') + key, values.join(QStringLiteral(", ")));
}

// TODOLATER: ?? this fn doesn't exist in python
QStringList KeyboardSettings::get_shortcuts(const QString& group, const QString& key,
                                           const QStringList& defaultValues)
{
    const QVariant v = value(group + QLatin1Char('/') + key);
    if (v.isValid()) {
        QStringList out;
        for (const QString& s : v.toString().split(QStringLiteral(", "))) {
            if (!s.isEmpty())
                out.append(s);
        }
        return out;
    }
    if (saveUnknownShortcuts)
        setShortcuts(group, key, defaultValues);
    return defaultValues;
}

void KeyboardSettings::setList(const QString& group, const QString& key,
                               const QStringList& values,
                               const QStringList& defaultValues)
{
    const QString full = group + QLatin1Char('/') + key;
    if (values == defaultValues)
        QSettings::remove(full);
    else
        setValue(full, values.join(QStringLiteral(", ")));
}

QStringList KeyboardSettings::getList(const QString& group, const QString& key,
                                      const QStringList& defaultValues) const
{
    const QVariant v = value(group + QLatin1Char('/') + key);
    if (!v.isValid())
        return defaultValues;
    QStringList out;
    for (const QString& s : v.toString().split(QStringLiteral(", "))) {
        if (!s.isEmpty())
            out.append(s);
    }
    return out;
}

void KeyboardSettings::setScalar(const QString& group, const QString& key,
                                  const QVariant& value, const QVariant& defaultValue)
{
    const QString full = group + QLatin1Char('/') + key;
    if (value == defaultValue)
        QSettings::remove(full);
    else
        QSettings::setValue(full, value);
}

QVariant KeyboardSettings::getScalar(const QString& group, const QString& key,
                                      const QVariant& defaultValue) const
{
    const QVariant v = value(group + QLatin1Char('/') + key);
    return v.isValid() ? v : defaultValue;
}

void KeyboardSettings::restoreDefaults()
{
    for (const QString& k : allKeys())
        QSettings::remove(k);
    emit SettingsEvents::instance().restoreKeyboardDefaults();
}

std::optional<ControlMatch> KeyboardSettings::mousewheelActionForEvent(
    const QWheelEvent* event) const
{
    for (const MouseWheelConfig& action : mousewheelActions()) {
        if (action.matchesEvent(event))
            return ControlMatch{action.group(), action.getInverted()};
    }
    return std::nullopt;
}

std::optional<ControlMatch> KeyboardSettings::mouseActionForEvent(
    const QMouseEvent* event) const
{
    for (const MouseConfig& action : mouseActions()) {
        if (action.matchesEvent(event))
            return ControlMatch{action.group(), action.getInverted()};
    }
    return std::nullopt;
}

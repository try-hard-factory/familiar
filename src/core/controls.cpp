#include "controls.h"
#include "settings.h"
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSet>
#include <QWheelEvent>

QString keyEventToSequenceString(const QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Control:
        return QStringLiteral("Ctrl");
    case Qt::Key_Shift:
        return QStringLiteral("Shift");
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
        return QStringLiteral("Alt");
    case Qt::Key_Meta:
        return QStringLiteral("Meta");
    default:
        break;
    }
    return QKeySequence(event->keyCombination()).toString();
}

// ─── Binding ──────────────────────────────────────────────────────────────────

QString Binding::displayText() const
{
    QStringList parts;
    if (!mouseButton.isEmpty())
        parts << mouseButton + QStringLiteral(" MB");
    if (!mouseModifiers.isEmpty()
        && mouseModifiers != QStringList{QStringLiteral("No Modifier")})
        parts << mouseModifiers.join(QLatin1Char('+'));
    if (!keySequence.isEmpty())
        parts << keySequence;

    // A wheel binding with no button/key and "No Modifier" (scroll alone
    // triggers it) would otherwise display as empty, indistinguishable
    // from "not configured at all" - spell it out instead.
    if (parts.isEmpty() && !mouseModifiers.isEmpty())
        parts << QStringLiteral("No Modifier");

    return parts.join(QStringLiteral(" + "));
}

QString Binding::serialize() const
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(keySequence,
             mouseButton,
             mouseModifiers.join(QLatin1Char('+')),
             inverted ? QStringLiteral("1") : QStringLiteral("0"),
             systemGlobal ? QStringLiteral("1") : QStringLiteral("0"));
}

Binding Binding::deserialize(const QString& s)
{
    const QStringList parts = s.split(QLatin1Char('|'));
    Binding b;
    if (parts.size() > 0)
        b.keySequence = parts[0];
    if (parts.size() > 1)
        b.mouseButton = parts[1];
    if (parts.size() > 2 && !parts[2].isEmpty())
        b.mouseModifiers = parts[2].split(QLatin1Char('+'));
    if (parts.size() > 3)
        b.inverted = parts[3] == QLatin1String("1");
    if (parts.size() > 4)
        b.systemGlobal = parts[4] == QLatin1String("1");
    return b;
}

// ─── MouseConfigBase ──────────────────────────────────────────────────────────

MouseConfigBase::MouseConfigBase(const QString& id,
                                 const QString& group,
                                 const QString& text,
                                 const QList<Binding>& defaultBindings,
                                 bool invertible)
    : id_(id)
    , group_(group)
    , text_(text)
    , defaultBindings_(defaultBindings)
    , invertible_(invertible)
{}

const QList<QPair<QString, Qt::KeyboardModifier>>& MouseConfigBase::modifierMap()
{
    static const QList<QPair<QString, Qt::KeyboardModifier>> map = {
        {"No Modifier", Qt::NoModifier},
        {"Shift", Qt::ShiftModifier},
        {"Ctrl", Qt::ControlModifier},
        {"Alt", Qt::AltModifier},
        {"Meta", Qt::MetaModifier},
        {"Keypad", Qt::KeypadModifier},
    };
    return map;
}

const QList<QPair<QString, Qt::MouseButton>>& MouseConfigBase::buttonMap()
{
    static const QList<QPair<QString, Qt::MouseButton>> map = {
        {"Not Configured", Qt::NoButton},
        {"Left", Qt::LeftButton},
        {"Middle", Qt::MiddleButton},
        {"Right", Qt::RightButton},
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

QList<Binding> MouseConfigBase::getBindings() const
{
    QStringList defaultSerialized;
    for (const Binding& b : defaultBindings_)
        defaultSerialized.append(b.serialize());

    const QStringList serialized
        = KeyboardSettings().getList(settingsGroup(),
                                     id_ + QStringLiteral("_bindings"),
                                     defaultSerialized);
    QList<Binding> out;
    for (const QString& s : serialized)
        out.append(Binding::deserialize(s));
    return out;
}

void MouseConfigBase::setBindings(const QList<Binding>& bindings) const
{
    QStringList serialized;
    for (const Binding& b : bindings)
        serialized.append(b.serialize());
    QStringList defaultSerialized;
    for (const Binding& b : defaultBindings_)
        defaultSerialized.append(b.serialize());

    KeyboardSettings().setList(settingsGroup(),
                               id_ + QStringLiteral("_bindings"),
                               serialized,
                               defaultSerialized);
}

QStringList MouseConfigBase::getModifiers() const
{
    return getBindings().value(0).mouseModifiers;
}

void MouseConfigBase::setModifiers(const QStringList& values) const
{
    QList<Binding> bindings = getBindings();
    if (bindings.isEmpty())
        bindings.append(Binding{});
    bindings[0].mouseModifiers = values;
    setBindings(bindings);
}

bool MouseConfigBase::getInverted() const
{
    return getBindings().value(0).inverted;
}

void MouseConfigBase::setInverted(bool value) const
{
    QList<Binding> bindings = getBindings();
    if (bindings.isEmpty())
        bindings.append(Binding{});
    bindings[0].inverted = value;
    setBindings(bindings);
}

// ─── MouseWheelConfig ─────────────────────────────────────────────────────────

MouseWheelConfig::MouseWheelConfig(const QString& id,
                                   const QString& group,
                                   const QString& text,
                                   const QList<Binding>& defaultBindings,
                                   bool invertible)
    : MouseConfigBase(id, group, text, defaultBindings, invertible)
{}

const char* MouseWheelConfig::settingsGroup() const
{
    return "MouseWheel";
}

bool MouseWheelConfig::controlsChanged() const
{
    return getBindings() != defaultBindings_;
}

bool MouseWheelConfig::isConfigured() const
{
    return !getBindings().isEmpty();
}

void MouseWheelConfig::removeControls() const
{
    setBindings({});
}

std::optional<Binding> MouseWheelConfig::matchesEvent(
    const QWheelEvent* event) const
{
    for (const Binding& b : getBindings()) {
        if (b.mouseModifiers.isEmpty() && b.keySequence.isEmpty())
            continue;

        Qt::KeyboardModifiers required = modifiersToQt(b.mouseModifiers);
        if (!b.keySequence.isEmpty()) {
            bool isBareModifier = false;
            for (const auto& pair : modifierMap()) {
                if (pair.first == b.keySequence) {
                    required |= pair.second;
                    isBareModifier = true;
                    break;
                }
            }
            if (!isBareModifier)
                continue;
        }

        if (required == event->modifiers())
            return b;
    }
    return std::nullopt;
}

// ─── MouseConfig ──────────────────────────────────────────────────────────────

MouseConfig::MouseConfig(const QString& id,
                         const QString& group,
                         const QString& text,
                         const QList<Binding>& defaultBindings,
                         bool invertible)
    : MouseConfigBase(id, group, text, defaultBindings, invertible)
{}

const char* MouseConfig::settingsGroup() const
{
    return "Mouse";
}

QString MouseConfig::getButton() const
{
    const QString btn = getBindings().value(0).mouseButton;
    return btn.isEmpty() ? QStringLiteral("Not Configured") : btn;
}

void MouseConfig::setButton(const QString& value) const
{
    QList<Binding> bindings = getBindings();
    if (bindings.isEmpty())
        bindings.append(Binding{});
    bindings[0].mouseButton
        = (value == QLatin1String("Not Configured")) ? QString() : value;
    setBindings(bindings);
}

QString MouseConfig::defaultButton() const
{
    const QString btn = defaultBindings_.value(0).mouseButton;
    return btn.isEmpty() ? QStringLiteral("Not Configured") : btn;
}

bool MouseConfig::controlsChanged() const
{
    return getBindings() != defaultBindings_;
}

bool MouseConfig::isConfigured() const
{
    return getButton() != QLatin1String("Not Configured");
}

void MouseConfig::removeControls() const
{
    setBindings({});
}

std::optional<Binding> MouseConfig::matchesEvent(const QMouseEvent* event) const
{
    const auto& bmap = buttonMap();
    for (const Binding& b : getBindings()) {
        if (b.mouseButton.isEmpty())
            continue;
        Qt::MouseButton btn = Qt::NoButton;
        for (const auto& [key, flag] : bmap) {
            if (key == b.mouseButton) {
                btn = flag;
                break;
            }
        }
        if (btn != event->button())
            continue;

        // The Mouse buttons field only ever captures the button itself
        // (see MouseButtonCaptureField) - a modifier held during the
        // click is captured separately, via the Keyboard keys field, as
        // a bare modifier name ("Alt", not a real key). Fold that into
        // the required modifier set alongside the legacy mouseModifiers
        // (still used by the hardcoded defaults, e.g. Zoom's Ctrl).
        Qt::KeyboardModifiers required = modifiersToQt(b.mouseModifiers);
        if (!b.keySequence.isEmpty()) {
            bool isBareModifier = false;
            for (const auto& pair : modifierMap()) {
                if (pair.first == b.keySequence) {
                    required |= pair.second;
                    isBareModifier = true;
                    break;
                }
            }
            // A real key (not a bare modifier) in keySequence isn't
            // something a mouse click alone can satisfy - skip it here.
            if (!isBareModifier)
                continue;
        }

        if (required == event->modifiers())
            return b;
    }
    return std::nullopt;
}

// ─── KeyboardSettings ─────────────────────────────────────────────────────────

KeyboardSettings::KeyboardSettings()
    : QSettings(QFileInfo(FamSettings().fileName())
                    .dir()
                    .filePath(QStringLiteral("KeyboardSettings.ini")),
                QSettings::IniFormat)
{}

const QList<MouseWheelConfig>& KeyboardSettings::mousewheelActions()
{
    // Plain scroll-to-zoom (no modifier) isn't listed here - it's not
    // user-configurable, see CanvasView::wheelEvent(). Everything else
    // (pan) needs a modifier held, so it stays a real Control.
    static const QList<MouseWheelConfig> list = {
        {"pan_horizontal",
         "pan_horizontal",
         "Pan horizontally",
         {Binding{{}, {}, {"Shift"}, true}},
         true},
        {"pan_vertical",
         "pan_vertical",
         "Pan vertically",
         {Binding{{}, {}, {"Shift", "Ctrl"}, true}},
         true},
    };
    return list;
}

const QList<MouseConfig>& KeyboardSettings::mouseActions()
{
    static const QList<MouseConfig> list = {
        {"zoom", "zoom", "Zoom", {Binding{{}, "Middle", {"Ctrl"}, false}}, true},
        {"pan",
         "pan",
         "Pan",
         {Binding{{}, "Left", {"Alt"}, false}},
         false},
        {"movewindow",
         "movewindow",
         "Move Window",
         {Binding{{}, "Left", {"Ctrl", "Alt"}, false}},
         false},
    };
    return list;
}

void KeyboardSettings::setShortcuts(const QString& group,
                                    const QString& key,
                                    const QStringList& values)
{
    setValue(group + QLatin1Char('/') + key, values.join(QStringLiteral(", ")));
}

// TODOLATER: ?? this fn doesn't exist in python
QStringList KeyboardSettings::get_shortcuts(const QString& group,
                                            const QString& key,
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

void KeyboardSettings::setList(const QString& group,
                               const QString& key,
                               const QStringList& values,
                               const QStringList& defaultValues)
{
    const QString full = group + QLatin1Char('/') + key;
    if (values == defaultValues)
        QSettings::remove(full);
    else
        setValue(full, values.join(QStringLiteral(", ")));
}

QStringList KeyboardSettings::getList(const QString& group,
                                      const QString& key,
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

void KeyboardSettings::setScalar(const QString& group,
                                 const QString& key,
                                 const QVariant& value,
                                 const QVariant& defaultValue)
{
    const QString full = group + QLatin1Char('/') + key;
    if (value == defaultValue)
        QSettings::remove(full);
    else
        QSettings::setValue(full, value);
}

QVariant KeyboardSettings::getScalar(const QString& group,
                                     const QString& key,
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
        if (auto binding = action.matchesEvent(event))
            return ControlMatch{action.group(), binding->inverted};
    }
    return std::nullopt;
}

std::optional<ControlMatch> KeyboardSettings::mouseActionForEvent(
    const QMouseEvent* event) const
{
    for (const MouseConfig& action : mouseActions()) {
        if (auto binding = action.matchesEvent(event))
            return ControlMatch{action.group(), binding->inverted};
    }
    return std::nullopt;
}

int KeyboardSettings::findConflictingMouseGroup(const QString& excludeId,
                                                const Binding& candidate) const
{
    if (candidate.mouseButton.isEmpty() && candidate.keySequence.isEmpty())
        return -1;
    const auto& list = mouseActions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id() == excludeId)
            continue;
        for (const Binding& b : list[i].getBindings()) {
            const bool mouseMatch = !candidate.mouseButton.isEmpty()
                && b.mouseButton == candidate.mouseButton
                && QSet<QString>(b.mouseModifiers.begin(), b.mouseModifiers.end())
                       == QSet<QString>(candidate.mouseModifiers.begin(),
                                        candidate.mouseModifiers.end());
            const bool keyMatch = !candidate.keySequence.isEmpty()
                && b.keySequence == candidate.keySequence;
            if (mouseMatch || keyMatch)
                return i;
        }
    }
    return -1;
}

int KeyboardSettings::findConflictingWheelGroup(const QString& excludeId,
                                                const Binding& candidate) const
{
    if (candidate.mouseModifiers.isEmpty() && candidate.keySequence.isEmpty())
        return -1;
    const auto& list = mousewheelActions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id() == excludeId)
            continue;
        for (const Binding& b : list[i].getBindings()) {
            const bool modMatch = !candidate.mouseModifiers.isEmpty()
                && QSet<QString>(b.mouseModifiers.begin(), b.mouseModifiers.end())
                       == QSet<QString>(candidate.mouseModifiers.begin(),
                                        candidate.mouseModifiers.end());
            const bool keyMatch = !candidate.keySequence.isEmpty()
                && b.keySequence == candidate.keySequence;
            if (modMatch || keyMatch)
                return i;
        }
    }
    return -1;
}

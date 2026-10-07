#include "controls.h"
#include "settings.h"
#include "settingshandler.h"
#include <actions/actions.h>
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QSet>
#include <QWheelEvent>

QString key_event_to_sequence_string(const QKeyEvent* event)
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

QString Binding::display_text() const
{
    QStringList parts;
    if (!mouseButton_.isEmpty()) {
        parts << mouseButton_ + QStringLiteral(" MB");
    }
    if (!mouseModifiers_.isEmpty()
        && mouseModifiers_ != QStringList{QStringLiteral("No Modifier")}) {
        parts << mouseModifiers_.join(QLatin1Char('+'));
    }
    if (!keySequence_.isEmpty()) {
        parts << keySequence_;
    }

    // A wheel binding with no button/key and "No Modifier" (scroll alone
    // triggers it) would otherwise display as empty, indistinguishable
    // from "not configured at all" - spell it out instead.
    if (parts.isEmpty() && !mouseModifiers_.isEmpty()) {
        parts << QStringLiteral("No Modifier");
    }

    return parts.join(QStringLiteral(" + "));
}

QString Binding::serialize() const
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(keySequence_,
             mouseButton_,
             mouseModifiers_.join(QLatin1Char('+')),
             inverted_ ? QStringLiteral("1") : QStringLiteral("0"),
             systemGlobal_ ? QStringLiteral("1") : QStringLiteral("0"));
}

Binding Binding::deserialize(const QString& s)
{
    const QStringList parts = s.split(QLatin1Char('|'));
    Binding b;
    if (parts.size() > 0) {
        b.set_key_sequence(parts[0]);
    }
    if (parts.size() > 1) {
        b.set_mouse_button(parts[1]);
    }
    if (parts.size() > 2 && !parts[2].isEmpty()) {
        b.set_mouse_modifiers(parts[2].split(QLatin1Char('+')));
    }
    if (parts.size() > 3) {
        b.set_inverted(parts[3] == QLatin1String("1"));
    }
    if (parts.size() > 4) {
        b.set_system_global(parts[4] == QLatin1String("1"));
    }
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

const QList<QPair<QString, Qt::KeyboardModifier>>& MouseConfigBase::modifier_map()
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

const QList<QPair<QString, Qt::MouseButton>>& MouseConfigBase::button_map()
{
    static const QList<QPair<QString, Qt::MouseButton>> map = {
        {"Not Configured", Qt::NoButton},
        {"Left", Qt::LeftButton},
        {"Middle", Qt::MiddleButton},
        {"Right", Qt::RightButton},
    };
    return map;
}

Qt::KeyboardModifiers MouseConfigBase::modifiers_to_qt(
    const QStringList& modifiers)
{
    Qt::KeyboardModifiers result = Qt::NoModifier;
    const auto& map = modifier_map();
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

QList<Binding> MouseConfigBase::get_bindings() const
{
    QStringList defaultSerialized;
    for (const Binding& b : defaultBindings_) {
        defaultSerialized.append(b.serialize());
    }

    const QStringList serialized
        = KeyboardSettings::get_list(settings_group(),
                                     id_ + QStringLiteral("_bindings"),
                                     defaultSerialized);
    QList<Binding> out;
    for (const QString& s : serialized) {
        out.append(Binding::deserialize(s));
    }
    return out;
}

void MouseConfigBase::set_bindings(const QList<Binding>& bindings) const
{
    QStringList serialized;
    for (const Binding& b : bindings) {
        serialized.append(b.serialize());
    }
    QStringList defaultSerialized;
    for (const Binding& b : defaultBindings_) {
        defaultSerialized.append(b.serialize());
    }

    KeyboardSettings::set_list(settings_group(),
                               id_ + QStringLiteral("_bindings"),
                               serialized,
                               defaultSerialized);
}

QStringList MouseConfigBase::get_modifiers() const
{
    return get_bindings().value(0).mouse_modifiers();
}

void MouseConfigBase::set_modifiers(const QStringList& values) const
{
    QList<Binding> bindings = get_bindings();
    if (bindings.isEmpty()) {
        bindings.append(Binding{});
    }
    bindings[0].set_mouse_modifiers(values);
    set_bindings(bindings);
}

bool MouseConfigBase::get_inverted() const
{
    return get_bindings().value(0).is_inverted();
}

void MouseConfigBase::set_inverted(bool value) const
{
    QList<Binding> bindings = get_bindings();
    if (bindings.isEmpty()) {
        bindings.append(Binding{});
    }
    bindings[0].set_inverted(value);
    set_bindings(bindings);
}

// ─── MouseWheelConfig ─────────────────────────────────────────────────────────

MouseWheelConfig::MouseWheelConfig(const QString& id,
                                   const QString& group,
                                   const QString& text,
                                   const QList<Binding>& defaultBindings,
                                   bool invertible)
    : MouseConfigBase(id, group, text, defaultBindings, invertible)
{}

const char* MouseWheelConfig::settings_group() const
{
    // Same JSON group as MouseConfig - both are "Controls" in the
    // Keyboard Shortcuts UI (widgets/controls/keyboard_shortcuts_page.h),
    // just two different C++ classes internally (wheel vs click
    // matching). ids don't collide between the two ("zoom"/"pan" vs
    // "pan_horizontal"/"pan_vertical"), so sharing one group is safe.
    return "Controls";
}

bool MouseWheelConfig::controls_changed() const
{
    return get_bindings() != default_bindings_ref();
}

bool MouseWheelConfig::is_configured() const
{
    return !get_bindings().isEmpty();
}

void MouseWheelConfig::remove_controls() const
{
    set_bindings({});
}

std::optional<Binding> MouseWheelConfig::matches_event(
    const QWheelEvent* event) const
{
    for (const Binding& b : get_bindings()) {
        if (b.mouse_modifiers().isEmpty() && b.key_sequence().isEmpty()) {
            continue;
        }

        Qt::KeyboardModifiers required = modifiers_to_qt(b.mouse_modifiers());
        if (!b.key_sequence().isEmpty()) {
            bool isBareModifier = false;
            for (const auto& pair : modifier_map()) {
                if (pair.first == b.key_sequence()) {
                    required |= pair.second;
                    isBareModifier = true;
                    break;
                }
            }
            if (!isBareModifier) {
                continue;
            }
        }

        if (required == event->modifiers()) {
            return b;
        }
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

const char* MouseConfig::settings_group() const
{
    // See MouseWheelConfig::settingsGroup() - shared "Controls" JSON group.
    return "Controls";
}

QString MouseConfig::get_button() const
{
    const QString btn = get_bindings().value(0).mouse_button();
    return btn.isEmpty() ? QStringLiteral("Not Configured") : btn;
}

void MouseConfig::set_button(const QString& value) const
{
    QList<Binding> bindings = get_bindings();
    if (bindings.isEmpty()) {
        bindings.append(Binding{});
    }
    bindings[0].set_mouse_button(
        value == QLatin1String("Not Configured") ? QString() : value);
    set_bindings(bindings);
}

QString MouseConfig::default_button() const
{
    const QString btn = default_bindings_ref().value(0).mouse_button();
    return btn.isEmpty() ? QStringLiteral("Not Configured") : btn;
}

bool MouseConfig::controls_changed() const
{
    return get_bindings() != default_bindings_ref();
}

bool MouseConfig::is_configured() const
{
    return get_button() != QLatin1String("Not Configured");
}

void MouseConfig::remove_controls() const
{
    set_bindings({});
}

std::optional<Binding> MouseConfig::matches_event(const QMouseEvent* event) const
{
    const auto& bmap = button_map();
    for (const Binding& b : get_bindings()) {
        if (b.mouse_button().isEmpty()) {
            continue;
        }
        Qt::MouseButton btn = Qt::NoButton;
        for (const auto& [key, flag] : bmap) {
            if (key == b.mouse_button()) {
                btn = flag;
                break;
            }
        }
        if (btn != event->button()) {
            continue;
        }

        // The Mouse buttons field only ever captures the button itself
        // (see MouseButtonCaptureField) - a modifier held during the
        // click is captured separately, via the Keyboard keys field, as
        // a bare modifier name ("Alt", not a real key). Fold that into
        // the required modifier set alongside the legacy mouseModifiers
        // (still used by the hardcoded defaults, e.g. Zoom's Ctrl).
        Qt::KeyboardModifiers required = modifiers_to_qt(b.mouse_modifiers());
        if (!b.key_sequence().isEmpty()) {
            bool isBareModifier = false;
            for (const auto& pair : modifier_map()) {
                if (pair.first == b.key_sequence()) {
                    required |= pair.second;
                    isBareModifier = true;
                    break;
                }
            }
            // A real key (not a bare modifier) in keySequence isn't
            // something a mouse click alone can satisfy - skip it here.
            if (!isBareModifier) {
                continue;
            }
        }

        if (required == event->modifiers()) {
            return b;
        }
    }
    return std::nullopt;
}

// ─── KeyboardSettings ─────────────────────────────────────────────────────────

const QList<MouseWheelConfig>& KeyboardSettings::mousewheel_actions()
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

const QList<MouseConfig>& KeyboardSettings::mouse_actions()
{
    // "movewindow" used to live here (Left + Ctrl+Alt) but was pure
    // decoration - nothing in the dispatch cascade (CanvasView::
    // mousePressEvent()) ever had a branch for match->group ==
    // "movewindow", so rebinding it in the Keyboard Shortcuts UI never
    // did anything. The app's actual move-the-window gesture is a
    // separate, hardcoded mechanism entirely (MainControlsMixin::
    // mousePressEventMainControls(), plain right-click-drag, not
    // configurable) - removed rather than wired up, since Max didn't
    // want a second, redundant way to do the same thing.
    static const QList<MouseConfig> list = {
        {"zoom", "zoom", "Zoom", {Binding{{}, "Middle", {"Ctrl"}, false}}, true},
        // Two default bindings for Pan - Alt+Left (original default) plus
        // a bare Middle-drag alias (no modifier needed) added by request,
        // matching the common "middle-mouse-button pans" convention other
        // canvas/image tools use. Doesn't collide with Zoom above - that
        // one requires Middle+Ctrl, this requires Middle with NO
        // modifiers held (MouseConfig::matchesEvent() matches modifiers
        // exactly, not as a subset).
        {"pan",
         "pan",
         "Pan",
         {Binding{{}, "Left", {"Alt"}, false}, Binding{{}, "Middle", {}, false}},
         false},
    };
    return list;
}

namespace {

QJsonArray to_json_array(const QStringList& values)
{
    QJsonArray arr;
    for (const QString& v : values) {
        arr.append(v);
    }
    return arr;
}

QStringList from_json_array(const QJsonValue& v)
{
    QStringList out;
    for (const QJsonValue& e : v.toArray()) {
        out.append(e.toString());
    }
    return out;
}

} // namespace

void KeyboardSettings::set_shortcuts(const QString& group,
                                     const QString& key,
                                     const QStringList& values)
{
    SettingsHandler::get_instance()->set_json_value(group,
                                                    key,
                                                    to_json_array(values));
}

// TODOLATER: ?? this fn doesn't exist in python
QStringList KeyboardSettings::get_shortcuts(const QString& group,
                                            const QString& key,
                                            const QStringList& defaultValues)
{
    const QJsonValue v = SettingsHandler::get_instance()->json_value(group, key);
    if (!v.isUndefined()) {
        return from_json_array(v);
    }
    if (saveUnknownShortcuts_) {
        set_shortcuts(group, key, defaultValues);
    }
    return defaultValues;
}

void KeyboardSettings::set_list(const QString& group,
                                const QString& key,
                                const QStringList& values,
                                const QStringList& defaultValues)
{
    if (values == defaultValues) {
        SettingsHandler::get_instance()->remove_json_value(group, key);
    } else {
        SettingsHandler::get_instance()->set_json_value(group,
                                                        key,
                                                        to_json_array(values));
    }
}

QStringList KeyboardSettings::get_list(const QString& group,
                                       const QString& key,
                                       const QStringList& defaultValues)
{
    const QJsonValue v = SettingsHandler::get_instance()->json_value(group, key);
    return v.isUndefined() ? defaultValues : from_json_array(v);
}

void KeyboardSettings::set_scalar(const QString& group,
                                  const QString& key,
                                  const QVariant& value,
                                  const QVariant& defaultValue)
{
    if (value == defaultValue) {
        SettingsHandler::get_instance()->remove_json_value(group, key);
    } else {
        SettingsHandler::get_instance()
            ->set_json_value(group, key, QJsonValue::fromVariant(value));
    }
}

QVariant KeyboardSettings::get_scalar(const QString& group,
                                      const QString& key,
                                      const QVariant& defaultValue)
{
    const QJsonValue v = SettingsHandler::get_instance()->json_value(group, key);
    return v.isUndefined() ? defaultValue : v.toVariant();
}

void KeyboardSettings::restore_defaults()
{
    SettingsHandler::get_instance()->remove_json_group(
        QStringLiteral("Actions"));
    SettingsHandler::get_instance()->remove_json_group(
        QStringLiteral("Controls"));
    // Clearing storage alone only fixes what Action::get_shortcuts()
    // returns on its NEXT call (e.g. the Keyboard Shortcuts settings
    // page's own rows, refreshed via restoreKeyboardDefaults() below) -
    // every already-built QAction (menu bar AND context_menu() -
    // ActionsMixin::_create_menu() adds the SAME QAction* to both, see
    // action_mixin.h) keeps showing whatever shortcut it was created/
    // last explicitly setShortcuts()'d with, since nothing else ever
    // re-pushes a fresh value into it. Refresh every action's live
    // QAction here too, straight to its (now unstored-again) code
    // default - NOT through Action::setShortcuts() (that persists
    // whatever it's given, which would defeat the whole point: a future
    // code-level default change should still reach anyone who's reset
    // here on their next launch, not pin them to today's default
    // forever).
    for (const Action* action : get_actions().all()) {
        if (!action->qaction()) {
            continue;
        }
        QList<QKeySequence> seqs;
        for (const QString& s : action->get_shortcuts()) {
            seqs.append(QKeySequence(s));
        }
        action->qaction()->setShortcuts(seqs);
    }
    emit SettingsEvents::instance().restore_keyboard_defaults();
}

std::optional<ControlMatch> KeyboardSettings::mousewheel_action_for_event(
    const QWheelEvent* event)
{
    for (const MouseWheelConfig& action : mousewheel_actions()) {
        if (auto binding = action.matches_event(event)) {
            return ControlMatch{.group = action.group(),
                                .inverted = binding->is_inverted()};
        }
    }
    return std::nullopt;
}

std::optional<ControlMatch> KeyboardSettings::mouse_action_for_event(
    const QMouseEvent* event)
{
    for (const MouseConfig& action : mouse_actions()) {
        if (auto binding = action.matches_event(event)) {
            return ControlMatch{.group = action.group(),
                                .inverted = binding->is_inverted()};
        }
    }
    return std::nullopt;
}

int KeyboardSettings::find_conflicting_mouse_group(const QString& excludeId,
                                                   const Binding& candidate)
{
    if (candidate.mouse_button().isEmpty()
        && candidate.key_sequence().isEmpty()) {
        return -1;
    }
    const auto& list = mouse_actions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id() == excludeId) {
            continue;
        }
        for (const Binding& b : list[i].get_bindings()) {
            const bool mouseMatch
                = !candidate.mouse_button().isEmpty()
                  && b.mouse_button() == candidate.mouse_button()
                  && QSet<QString>(b.mouse_modifiers().begin(),
                                   b.mouse_modifiers().end())
                         == QSet<QString>(candidate.mouse_modifiers().begin(),
                                          candidate.mouse_modifiers().end());
            const bool keyMatch = !candidate.key_sequence().isEmpty()
                                  && b.key_sequence()
                                         == candidate.key_sequence();
            if (mouseMatch || keyMatch) {
                return i;
            }
        }
    }
    return -1;
}

int KeyboardSettings::find_conflicting_wheel_group(const QString& excludeId,
                                                   const Binding& candidate)
{
    if (candidate.mouse_modifiers().isEmpty()
        && candidate.key_sequence().isEmpty()) {
        return -1;
    }
    const auto& list = mousewheel_actions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id() == excludeId) {
            continue;
        }
        for (const Binding& b : list[i].get_bindings()) {
            const bool modMatch
                = !candidate.mouse_modifiers().isEmpty()
                  && QSet<QString>(b.mouse_modifiers().begin(),
                                   b.mouse_modifiers().end())
                         == QSet<QString>(candidate.mouse_modifiers().begin(),
                                          candidate.mouse_modifiers().end());
            const bool keyMatch = !candidate.key_sequence().isEmpty()
                                  && b.key_sequence()
                                         == candidate.key_sequence();
            if (modMatch || keyMatch) {
                return i;
            }
        }
    }
    return -1;
}

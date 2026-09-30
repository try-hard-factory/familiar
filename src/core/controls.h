#ifndef CONTROLS_H
#define CONTROLS_H

#include <optional>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>

class QWheelEvent;
class QMouseEvent;
class QKeyEvent;

// Converts a raw keyPressEvent to the string format Binding::keySequence
// uses. Mostly QKeySequence(event->keyCombination()).toString(), except a
// BARE modifier press (just Ctrl, just Alt, ...) - QKeySequence has no
// representation for "modifier alone, no key", but it's a real, useful
// trigger (e.g. "hold Alt to pan"), so those are special-cased to a plain
// name ("Ctrl"/"Shift"/"Alt"/"Meta"). Used by both the capture field that
// records a binding and the dispatchers that match a live key press
// against one, so they agree on the same strings.
QString key_event_to_sequence_string(const QKeyEvent* event);

// ─── Binding ──────────────────────────────────────────────────────────────────

// One alias: a keyboard shortcut and/or a mouse-button chord that both
// trigger the same target (Action, MouseConfig, or MouseWheelConfig). Either
// part may be empty; a Controls (mouse) binding never has keySequence set
// today, and an Action's keyboard-only binding never has mouseButton set -
// the combination of both (e.g. "hold Middle mouse button, press F") is
// reserved for a later phase (see memory/familiar_next_steps.md step 6).
struct Binding
{
    QString keySequence;
    QString mouseButton;
    QStringList mouseModifiers;
    bool inverted = false;
    bool systemGlobal
        = false; // stored for forward compat; no dispatch effect yet

    bool is_empty() const
    {
        return keySequence.isEmpty() && mouseButton.isEmpty();
    }
    bool is_keyboard_only() const
    {
        return mouseButton.isEmpty() && !keySequence.isEmpty();
    }
    bool is_mouse_only() const
    {
        return !mouseButton.isEmpty() && keySequence.isEmpty();
    }
    bool is_mixed() const
    {
        return !mouseButton.isEmpty() && !keySequence.isEmpty();
    }

    // Chip label, e.g. "Ctrl+S", "Middle MB", "Left MB + Ctrl+Alt+Shift".
    QString display_text() const;

    QString serialize() const;
    static Binding deserialize(const QString& s);

    bool operator==(const Binding& o) const
    {
        return keySequence == o.keySequence && mouseButton == o.mouseButton
               && mouseModifiers == o.mouseModifiers && inverted == o.inverted
               && systemGlobal == o.systemGlobal;
    }
};

// ─── MouseConfigBase ──────────────────────────────────────────────────────────

class MouseConfigBase
{
public:
    virtual ~MouseConfigBase() = default;

    virtual const QString& id() const = 0;
    virtual const QString& group() const = 0;
    virtual const QString& text() const = 0;
    virtual const char* settings_group() const = 0;

    virtual bool controls_changed() const = 0;
    virtual bool is_configured() const = 0;
    virtual void remove_controls() const = 0;

    bool is_invertible() const { return invertible_; }
    bool default_inverted() const { return defaultBindings_.value(0).inverted; }
    QStringList default_modifiers() const
    {
        return defaultBindings_.value(0).mouseModifiers;
    }
    virtual QString default_button() const { return {}; }

    bool operator==(const MouseConfigBase& o) const { return id() == o.id(); }

    // Ordered modifier name → Qt flag mapping.
    static const QList<QPair<QString, Qt::KeyboardModifier>>& modifier_map();
    // Ordered button name → Qt flag mapping.
    static const QList<QPair<QString, Qt::MouseButton>>& button_map();
    // Convert list of modifier names to combined Qt::KeyboardModifiers.
    static Qt::KeyboardModifiers modifiers_to_qt(const QStringList& modifiers);

    // N-alias API.
    QList<Binding> get_bindings() const;
    void set_bindings(const QList<Binding>& bindings) const;
    const QList<Binding>& default_bindings() const { return defaultBindings_; }

    // Single-binding API kept for the existing Mouse/Mouse Wheel table
    // widgets (widgets/controls/mouse_controls.cpp,
    // mousewheel_controls.cpp) - thin wrappers over getBindings()[0], see
    // core/controls.cpp.
    QStringList get_modifiers() const;
    void set_modifiers(const QStringList& values) const;
    bool get_inverted() const;
    void set_inverted(bool value) const;

protected:
    MouseConfigBase(const QString& id,
                    const QString& group,
                    const QString& text,
                    const QList<Binding>& defaultBindings,
                    bool invertible);

    QString id_;
    QString group_;
    QString text_;
    QList<Binding> defaultBindings_;
    bool invertible_;
};

// ─── MouseWheelConfig ─────────────────────────────────────────────────────────

class MouseWheelConfig : public MouseConfigBase
{
public:
    MouseWheelConfig(const QString& id,
                     const QString& group,
                     const QString& text,
                     const QList<Binding>& defaultBindings,
                     bool invertible);

    const QString& id() const override { return id_; }
    const QString& group() const override { return group_; }
    const QString& text() const override { return text_; }
    const char* settings_group() const override;

    bool controls_changed() const override;
    bool is_configured() const override;
    void remove_controls() const override;
    std::optional<Binding> matches_event(const QWheelEvent* event) const;
};

// ─── MouseConfig ──────────────────────────────────────────────────────────────

class MouseConfig : public MouseConfigBase
{
public:
    MouseConfig(const QString& id,
                const QString& group,
                const QString& text,
                const QList<Binding>& defaultBindings,
                bool invertible);

    const QString& id() const override { return id_; }
    const QString& group() const override { return group_; }
    const QString& text() const override { return text_; }
    const char* settings_group() const override;

    // "Not Configured" if the primary (index-0) binding has no mouse button.
    QString get_button() const;
    void set_button(const QString& value) const;
    QString default_button() const override;

    bool controls_changed() const override;
    bool is_configured() const override;
    void remove_controls() const override;
    std::optional<Binding> matches_event(const QMouseEvent* event) const;
};

// ─── KeyboardSettings ─────────────────────────────────────────────────────────

struct ControlMatch
{
    QString group;
    bool inverted = false;
};

// Thin value-typed facade over the "Actions"/"Mouse"/"MouseWheel" groups
// of the single JSON document owned by SettingsHandler
// (core/settingshandler.h) - same role as FamSettings (core/settings.h)
// for its own groups. Constructed fresh at each call site, holds no
// state of its own.
class KeyboardSettings
{
public:
    KeyboardSettings() = default;

    static const QList<MouseWheelConfig>& mousewheel_actions();
    static const QList<MouseConfig>& mouse_actions();

    // ── Shortcut API (used by Action) ─────────────────────────────────────────
    // Saves even if equal to default (saveUnknownShortcuts flag controls this).
    void set_shortcuts(const QString& group,
                      const QString& key,
                      const QStringList& values);
    QStringList get_shortcuts(const QString& group,
                              const QString& key,
                              const QStringList& defaultValues = {});

    // ── Generic list API (used by mouse/wheel configs) ────────────────────────
    // Removes key when values == defaultValues (stores only non-default data).
    void set_list(const QString& group,
                 const QString& key,
                 const QStringList& values,
                 const QStringList& defaultValues = {});
    QStringList get_list(const QString& group,
                        const QString& key,
                        const QStringList& defaultValues = {}) const;

    // ── Generic scalar API (used by mouse/wheel configs) ──────────────────────
    void set_scalar(const QString& group,
                   const QString& key,
                   const QVariant& value,
                   const QVariant& defaultValue = {});
    QVariant get_scalar(const QString& group,
                       const QString& key,
                       const QVariant& defaultValue = {}) const;

    // Removes all stored controls and emits SettingsEvents::restoreKeyboardDefaults.
    void restore_defaults();

    std::optional<ControlMatch> mousewheel_action_for_event(
        const QWheelEvent* event) const;
    std::optional<ControlMatch> mouse_action_for_event(
        const QMouseEvent* event) const;

    // Index into mouseActions()/mousewheelActions() of a group (other than
    // excludeId) whose bindings already use the same button+modifiers as
    // `candidate`, or -1 if none. Used by both the old single-binding
    // Mouse/Mouse Wheel dialogs and the new alias dialogs.
    int find_conflicting_mouse_group(const QString& excludeId,
                                  const Binding& candidate) const;
    int find_conflicting_wheel_group(const QString& excludeId,
                                  const Binding& candidate) const;

    bool saveUnknownShortcuts = true;
};

#endif // CONTROLS_H

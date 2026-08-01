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
QString keyEventToSequenceString(const QKeyEvent* event);

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
    bool systemGlobal = false; // stored for forward compat; no dispatch effect yet

    bool isEmpty() const { return keySequence.isEmpty() && mouseButton.isEmpty(); }
    bool isKeyboardOnly() const
    {
        return mouseButton.isEmpty() && !keySequence.isEmpty();
    }
    bool isMouseOnly() const
    {
        return !mouseButton.isEmpty() && keySequence.isEmpty();
    }
    bool isMixed() const
    {
        return !mouseButton.isEmpty() && !keySequence.isEmpty();
    }

    // Chip label, e.g. "Ctrl+S", "Middle MB", "Left MB + Ctrl+Alt+Shift".
    QString displayText() const;

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
    virtual const char* settingsGroup() const = 0;

    virtual bool controlsChanged() const = 0;
    virtual bool isConfigured() const = 0;
    virtual void removeControls() const = 0;

    bool isInvertible() const { return invertible_; }
    bool defaultInverted() const { return defaultBindings_.value(0).inverted; }
    QStringList defaultModifiers() const
    {
        return defaultBindings_.value(0).mouseModifiers;
    }
    virtual QString defaultButton() const { return {}; }

    bool operator==(const MouseConfigBase& o) const { return id() == o.id(); }

    // Ordered modifier name → Qt flag mapping.
    static const QList<QPair<QString, Qt::KeyboardModifier>>& modifierMap();
    // Ordered button name → Qt flag mapping.
    static const QList<QPair<QString, Qt::MouseButton>>& buttonMap();
    // Convert list of modifier names to combined Qt::KeyboardModifiers.
    static Qt::KeyboardModifiers modifiersToQt(const QStringList& modifiers);

    // N-alias API.
    QList<Binding> getBindings() const;
    void setBindings(const QList<Binding>& bindings) const;
    const QList<Binding>& defaultBindings() const { return defaultBindings_; }

    // Single-binding API kept for the existing Mouse/Mouse Wheel table
    // widgets (widgets/controls/mouse_controls.cpp,
    // mousewheel_controls.cpp) - thin wrappers over getBindings()[0], see
    // core/controls.cpp.
    QStringList getModifiers() const;
    void setModifiers(const QStringList& values) const;
    bool getInverted() const;
    void setInverted(bool value) const;

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
    const char* settingsGroup() const override;

    bool controlsChanged() const override;
    bool isConfigured() const override;
    void removeControls() const override;
    std::optional<Binding> matchesEvent(const QWheelEvent* event) const;
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
    const char* settingsGroup() const override;

    // "Not Configured" if the primary (index-0) binding has no mouse button.
    QString getButton() const;
    void setButton(const QString& value) const;
    QString defaultButton() const override;

    bool controlsChanged() const override;
    bool isConfigured() const override;
    void removeControls() const override;
    std::optional<Binding> matchesEvent(const QMouseEvent* event) const;
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

    static const QList<MouseWheelConfig>& mousewheelActions();
    static const QList<MouseConfig>& mouseActions();

    // ── Shortcut API (used by Action) ─────────────────────────────────────────
    // Saves even if equal to default (saveUnknownShortcuts flag controls this).
    void setShortcuts(const QString& group,
                      const QString& key,
                      const QStringList& values);
    QStringList get_shortcuts(const QString& group,
                              const QString& key,
                              const QStringList& defaultValues = {});

    // ── Generic list API (used by mouse/wheel configs) ────────────────────────
    // Removes key when values == defaultValues (stores only non-default data).
    void setList(const QString& group,
                 const QString& key,
                 const QStringList& values,
                 const QStringList& defaultValues = {});
    QStringList getList(const QString& group,
                        const QString& key,
                        const QStringList& defaultValues = {}) const;

    // ── Generic scalar API (used by mouse/wheel configs) ──────────────────────
    void setScalar(const QString& group,
                   const QString& key,
                   const QVariant& value,
                   const QVariant& defaultValue = {});
    QVariant getScalar(const QString& group,
                       const QString& key,
                       const QVariant& defaultValue = {}) const;

    // Removes all stored controls and emits SettingsEvents::restoreKeyboardDefaults.
    void restoreDefaults();

    std::optional<ControlMatch> mousewheelActionForEvent(
        const QWheelEvent* event) const;
    std::optional<ControlMatch> mouseActionForEvent(
        const QMouseEvent* event) const;

    // Index into mouseActions()/mousewheelActions() of a group (other than
    // excludeId) whose bindings already use the same button+modifiers as
    // `candidate`, or -1 if none. Used by both the old single-binding
    // Mouse/Mouse Wheel dialogs and the new alias dialogs.
    int findConflictingMouseGroup(const QString& excludeId,
                                  const Binding& candidate) const;
    int findConflictingWheelGroup(const QString& excludeId,
                                  const Binding& candidate) const;

    bool saveUnknownShortcuts = true;
};

#endif // CONTROLS_H

#pragma once

#include <core/controls.h>

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class QAction;

struct Action
{
public:
    static constexpr const char* ksettingsGroup = "Actions";

    const QString& id() const { return id_; }
    const QString& text() const { return text_; }
    // Slot name on the host widget, invoked by name through
    // QMetaObject::invokeMethod (see ActionsMixin).
    const QString& callback() const { return callback_; }
    // The compiled-in defaults. get_shortcuts() below is the different
    // thing: what the user actually has, settings override included.
    const QStringList& default_shortcuts() const { return shortcuts_; }
    bool is_checkable() const { return checkable_; }
    // Initial state only - once qaction() exists, it owns the live one.
    bool initially_checked() const { return checked_; }
    // Action group name; empty = no group.
    const QString& group() const { return group_; }
    // Settings key for persisting checkable state.
    const QString& settings_key() const { return settingsKey_; }
    bool is_enabled() const { return enabled_; }
    // Builder id for dynamic submenus.
    const QString& menu_id() const { return menuId_; }
    QAction* qaction() const { return qaction_; }
    void set_qaction(QAction* action) { qaction_ = action; }

    // Convenience factory — improves readability at the call site.
    static Action make(const QString& id,
                       const QString& text,
                       const QString& callback = {},
                       const QStringList& shortcuts = {},
                       bool checkable = false,
                       bool checked = false,
                       const QString& group = {},
                       const QString& settingsKey = {},
                       bool enabled = true,
                       const QString& menuId = {});

    QStringList get_shortcuts() const;
    void set_shortcuts(const QStringList& values);
    QKeySequence get_key_sequence(int index) const;
    bool shortcuts_changed() const;
    QString get_default_shortcut(int index) const;

    // Mouse-chord and mixed mouse+key aliases - a separate, additive
    // store (key "Actions/<id>_mouse") from the plain keyboard shortcuts
    // above; dispatched by ActionMouseDispatcher (actions/
    // action_mouse_dispatch.h), not by Qt's native QAction::setShortcuts().
    QList<Binding> get_mouse_bindings() const;
    void set_mouse_bindings(const QList<Binding>& values);

    // text with the Qt mnemonic marker stripped - a lone '&' is removed,
    // but '&&' (escaped, meant to display as a literal '&') is kept.
    QString display_text() const;

private:
    QString id_;
    QString text_;
    QString callback_;
    QStringList shortcuts_;
    bool checkable_ = false;
    bool checked_ = false;
    QString group_;
    QString settingsKey_;
    bool enabled_ = true;
    QString menuId_;
    QAction* qaction_ = nullptr;
};

// Insertion-ordered registry (QMap for O(log n) lookup, QList for order).
class ActionRegistry
{
public:
    void add(Action action); // upsert
    Action& operator[](const QString& id);
    Action* find(const QString& id); // nullptr if missing
    void remove(const QString& id);
    bool contains(const QString& id) const;
    QList<Action*> all();     // in insertion order
    QStringList keys() const; // in insertion order

    // Action (other than excludeId) whose shortcuts already contain
    // `shortcut`, or nullptr if none.
    Action* find_by_shortcut(const QString& excludeId, const QString& shortcut);

    // Action (other than excludeId) whose mouse bindings already use the
    // same button+modifiers as `candidate`, or nullptr if none.
    Action* find_by_mouse_binding(const QString& excludeId,
                                  const Binding& candidate);

private:
    QList<QString> order_;
    QMap<QString, Action> map_;
};

// Global singleton registry populated at first call.
ActionRegistry& get_actions();

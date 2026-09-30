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
    static constexpr const char* ksettingsGroup = "Actions";

    QString id;
    QString text;
    QString callback;      // slot name on the host widget
    QStringList shortcuts; // default shortcuts
    bool checkable = false;
    bool checked = false; // initial checked state
    QString group;        // action group name; empty = no group
    QString settingsKey;  // Settings key for persisting checkable state
    bool enabled = true;
    QString menuId; // builder id for dynamic submenus
    QAction* qaction = nullptr;

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

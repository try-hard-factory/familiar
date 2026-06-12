#pragma once

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class QAction;

struct Action {
    static constexpr const char* SETTINGS_GROUP = "Actions";

    QString id;
    QString text;
    QString callback;      // slot name on the host widget
    QStringList shortcuts; // default shortcuts
    bool checkable = false;
    bool checked = false;  // initial checked state
    QString group;         // action group name; empty = no group
    QString settingsKey;   // BeeSettings key for persisting checkable state
    bool enabled = true;
    QString menuId;        // builder id for dynamic submenus
    QAction* qaction = nullptr;

    // Convenience factory — improves readability at the call site.
    static Action make(
        const QString& id, const QString& text,
        const QString& callback = {},
        const QStringList& shortcuts = {},
        bool checkable = false, bool checked = false,
        const QString& group = {}, const QString& settingsKey = {},
        bool enabled = true, const QString& menuId = {});

    QStringList getShortcuts() const;
    void setShortcuts(const QStringList& values);
    QKeySequence getKeySequence(int index) const;
    bool shortcutsChanged() const;
    QString getDefaultShortcut(int index) const;
};

// Insertion-ordered registry (QMap for O(log n) lookup, QList for order).
class ActionRegistry
{
public:
    void add(Action action);              // upsert
    Action& operator[](const QString& id);
    Action* find(const QString& id);      // nullptr if missing
    void remove(const QString& id);
    bool contains(const QString& id) const;
    QList<Action*> all();                 // in insertion order
    QStringList keys() const;             // in insertion order

private:
    QList<QString> order_;
    QMap<QString, Action> map_;
};

// Global singleton registry populated at first call.
ActionRegistry& getActions();

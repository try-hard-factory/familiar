#pragma once

#include <QList>
#include <QString>

struct MenuNode {
    enum class Type { Action, Separator, Submenu, Dynamic };

    Type type;
    QString label;             // for Submenu
    QString id;                // for Action, Dynamic
    QList<MenuNode> children;  // for Submenu

    static MenuNode sep()
    {
        return {Type::Separator, {}, {}, {}};
    }
    static MenuNode action(const QString& id)
    {
        return {Type::Action, {}, id, {}};
    }
    static MenuNode submenu(const QString& label, QList<MenuNode> children)
    {
        return {Type::Submenu, label, {}, std::move(children)};
    }
    static MenuNode dynamic(const QString& builderId)
    {
        return {Type::Dynamic, {}, builderId, {}};
    }
};

const QList<MenuNode>& menuStructure();

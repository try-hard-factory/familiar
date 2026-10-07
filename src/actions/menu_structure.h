#pragma once

#include <QList>
#include <QString>

struct MenuNode
{
    enum class Type { Action, Separator, Submenu, Dynamic };

    Type type;
    QString label;            // for Submenu
    QString id;               // for Action, Dynamic
    QList<MenuNode> children; // for Submenu

    static MenuNode sep()
    {
        return {.type = Type::Separator, .label = {}, .id = {}, .children = {}};
    }
    static MenuNode action(const QString& id)
    {
        return {.type = Type::Action, .label = {}, .id = id, .children = {}};
    }
    static MenuNode submenu(const QString& label, QList<MenuNode> children)
    {
        return {.type = Type::Submenu,
                .label = label,
                .id = {},
                .children = std::move(children)};
    }
    static MenuNode dynamic(const QString& builderId)
    {
        return {.type = Type::Dynamic,
                .label = {},
                .id = builderId,
                .children = {}};
    }
};

const QList<MenuNode>& menu_structure();

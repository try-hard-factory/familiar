#include "actions/menu_structure.h"

#include <gtest/gtest.h>

TEST(MenuNodeTest, FactoryMethodsPopulateExpectedFields)
{
    const MenuNode sep = MenuNode::sep();
    EXPECT_EQ(sep.type, MenuNode::Type::Separator);

    const MenuNode action = MenuNode::action(QStringLiteral("open"));
    EXPECT_EQ(action.type, MenuNode::Type::Action);
    EXPECT_EQ(action.id, QStringLiteral("open"));

    const MenuNode dynamic = MenuNode::dynamic(QStringLiteral("recent_files"));
    EXPECT_EQ(dynamic.type, MenuNode::Type::Dynamic);
    EXPECT_EQ(dynamic.id, QStringLiteral("recent_files"));

    const MenuNode submenu = MenuNode::submenu(
        QStringLiteral("File"), {MenuNode::action(QStringLiteral("open"))});
    EXPECT_EQ(submenu.type, MenuNode::Type::Submenu);
    EXPECT_EQ(submenu.label, QStringLiteral("File"));
    ASSERT_EQ(submenu.children.size(), 1);
    EXPECT_EQ(submenu.children.first().id, QStringLiteral("open"));
}

namespace {
// Recursively checks the invariants menuStructure()'s consumers
// (ActionsMixin::_create_menu() and friends) implicitly rely on -
// doesn't hardcode the tree's actual shape (which would break on every
// menu edit), just that it stays well-formed.
void checkWellFormed(const MenuNode& node)
{
    switch (node.type) {
    case MenuNode::Type::Action:
    case MenuNode::Type::Dynamic:
        EXPECT_FALSE(node.id.isEmpty());
        break;
    case MenuNode::Type::Submenu:
        EXPECT_FALSE(node.label.isEmpty());
        for (const MenuNode& child : node.children) {
            checkWellFormed(child);
        }
        break;
    case MenuNode::Type::Separator:
        break;
    }
}
} // namespace

TEST(MenuStructureTest, EveryNodeIsWellFormed)
{
    const QList<MenuNode>& top = menuStructure();
    EXPECT_FALSE(top.isEmpty());
    for (const MenuNode& node : top) {
        checkWellFormed(node);
    }
}

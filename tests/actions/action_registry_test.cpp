#include "actions/actions.h"

#include "core/settingshandler.h"

#include <gtest/gtest.h>

// A LOCAL ActionRegistry, not the global getActions() singleton - plain
// default-constructible class, no need to touch (or populate) the
// app-wide registry just to test its own add/find/remove/lookup logic.

TEST(ActionRegistryTest, AddFindContainsRemove)
{
    ActionRegistry registry;
    EXPECT_FALSE(registry.contains(QStringLiteral("a")));

    registry.add(Action::make(QStringLiteral("a"), QStringLiteral("Action A")));
    registry.add(Action::make(QStringLiteral("b"), QStringLiteral("Action B")));

    EXPECT_TRUE(registry.contains(QStringLiteral("a")));
    ASSERT_NE(registry.find(QStringLiteral("a")), nullptr);
    EXPECT_EQ(registry.find(QStringLiteral("a"))->text,
             QStringLiteral("Action A"));
    EXPECT_EQ(registry.find(QStringLiteral("nonexistent")), nullptr);

    registry.remove(QStringLiteral("a"));
    EXPECT_FALSE(registry.contains(QStringLiteral("a")));
}

TEST(ActionRegistryTest, AddIsUpsertAndPreservesInsertionOrder)
{
    ActionRegistry registry;
    registry.add(Action::make(QStringLiteral("a"), QStringLiteral("First")));
    registry.add(Action::make(QStringLiteral("b"), QStringLiteral("Second")));
    // Re-adding an existing id updates it in place, not a new entry at
    // the end.
    registry.add(
        Action::make(QStringLiteral("a"), QStringLiteral("First Updated")));

    EXPECT_EQ(registry.keys(),
             (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
    EXPECT_EQ(registry.find(QStringLiteral("a"))->text,
             QStringLiteral("First Updated"));
    EXPECT_EQ(registry.all().size(), 2);
}

TEST(ActionRegistryTest, FindByShortcutExcludesGivenIdAndEmptyShortcut)
{
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), QStringLiteral("reg_test_a"));
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), QStringLiteral("reg_test_b"));

    ActionRegistry registry;
    registry.add(Action::make(QStringLiteral("reg_test_a"),
                              QStringLiteral("A"),
                              {},
                              {QStringLiteral("Ctrl+K")}));
    registry.add(Action::make(QStringLiteral("reg_test_b"),
                              QStringLiteral("B"),
                              {},
                              {QStringLiteral("Ctrl+L")}));

    EXPECT_EQ(registry
                 .findByShortcut(QStringLiteral("reg_test_b"),
                                 QStringLiteral("Ctrl+K"))
                 ->id,
             QStringLiteral("reg_test_a"));
    // Excluding the owning action itself finds nothing, even though its
    // shortcut matches.
    EXPECT_EQ(registry.findByShortcut(QStringLiteral("reg_test_a"),
                                     QStringLiteral("Ctrl+K")),
             nullptr);
    // Empty shortcut short-circuits to nullptr (findByShortcut()'s own
    // guard) rather than matching an action with no shortcuts at all.
    EXPECT_EQ(registry.findByShortcut(QStringLiteral("reg_test_b"), QString()),
             nullptr);

    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), QStringLiteral("reg_test_a"));
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), QStringLiteral("reg_test_b"));
}

TEST(ActionRegistryTest, FindByMouseBindingMatchesButtonAndModifierSet)
{
    Action a
        = Action::make(QStringLiteral("reg_test_mouse_a"), QStringLiteral("A"));
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), a.id + QStringLiteral("_mouse"));

    Binding stored;
    stored.mouseButton = QStringLiteral("Middle");
    stored.mouseModifiers = {QStringLiteral("Ctrl")};
    a.setMouseBindings({stored});

    ActionRegistry registry;
    registry.add(a);

    Binding candidate;
    candidate.mouseButton = QStringLiteral("Middle");
    candidate.mouseModifiers = {QStringLiteral("Ctrl")};

    EXPECT_EQ(
        registry.findByMouseBinding(QStringLiteral("other"), candidate)->id,
        QStringLiteral("reg_test_mouse_a"));
    EXPECT_EQ(registry.findByMouseBinding(QStringLiteral("reg_test_mouse_a"),
                                         candidate),
             nullptr);

    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), a.id + QStringLiteral("_mouse"));
}

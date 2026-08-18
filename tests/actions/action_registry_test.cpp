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

// getActions() is the app-wide registry (a function-local static, built
// once from buildRegistry()'s ~350-line literal table of every action in
// the app - actions/actions.cpp). It's otherwise never exercised by any
// test in this binary (see keyboard_shortcuts_page_test.cpp's own
// comment) - this doesn't check the table's actual contents (that would
// break on every menu edit), just the structural invariants
// ActionsMixin's QMetaObject::invokeMethod-by-name reflection and the
// UI layer implicitly rely on: no action silently missing an id/text.
// Calling getActions() here does permanently populate the process-wide
// singleton for the rest of this test binary's run (unlike the ids used
// throughout this file, which are cleaned up - there's no "un-build" for
// a function-local static) - harmless, since nothing else in this binary
// asserts the registry stays empty.
TEST(GlobalActionRegistryTest, EveryRegisteredActionHasNonEmptyIdAndText)
{
    const QList<Action*> all = getActions().all();
    EXPECT_FALSE(all.isEmpty());
    for (const Action* a : all) {
        EXPECT_FALSE(a->id.isEmpty());
        EXPECT_FALSE(a->text.isEmpty()) << "action id: " << a->id.toStdString();
        // Every default shortcut string must parse as a real QKeySequence -
        // getKeySequence()/get_shortcuts() are how every keybinding path
        // (ActionMouseDispatcher, KeyboardShortcutsPage) reads these back.
        for (int i = 0; i < a->shortcuts.size(); ++i) {
            EXPECT_FALSE(a->getKeySequence(i).isEmpty())
                << "action id: " << a->id.toStdString();
        }
    }
    EXPECT_EQ(getActions().keys().size(), all.size());
}

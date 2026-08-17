#include "actions/actions.h"

#include "core/settingshandler.h"

#include <gtest/gtest.h>

namespace {
// Action's shortcuts/mouse bindings are stored keyed by its own `id` in
// the "Actions" JSON group (SettingsHandler's storage, redirected to a
// throwaway temp file by tests/support/settings_test_environment.h) -
// clears both keys so each test starts clean regardless of what an
// earlier one using the same id left behind.
void cleanupAction(const QString& id)
{
    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                    id);
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), id + QStringLiteral("_mouse"));
}
} // namespace

TEST(ActionTest, DisplayTextStripsMnemonicButKeepsEscapedAmpersand)
{
    Action a = Action::make(QStringLiteral("dt_a"),
                            QStringLiteral("&Open Recent"));
    EXPECT_EQ(a.displayText(), QStringLiteral("Open Recent"));

    Action b = Action::make(QStringLiteral("dt_b"),
                            QStringLiteral("Save && Close"));
    EXPECT_EQ(b.displayText(), QStringLiteral("Save & Close"));
}

TEST(ActionTest, GetShortcutsFallsBackToDefaultThenPersistsOverride)
{
    Action a = Action::make(QStringLiteral("test_action_shortcuts"),
                            QStringLiteral("Test"),
                            {},
                            {QStringLiteral("Ctrl+T")});
    cleanupAction(a.id);

    EXPECT_EQ(a.get_shortcuts(), QStringList{QStringLiteral("Ctrl+T")});
    EXPECT_FALSE(a.shortcutsChanged());

    a.setShortcuts({QStringLiteral("Ctrl+Shift+T")});
    EXPECT_EQ(a.get_shortcuts(),
             QStringList{QStringLiteral("Ctrl+Shift+T")});
    EXPECT_TRUE(a.shortcutsChanged());

    cleanupAction(a.id);
}

TEST(ActionTest, GetKeySequenceAndDefaultShortcutAreIndexBased)
{
    Action a = Action::make(QStringLiteral("test_action_idx"),
                            QStringLiteral("Test"),
                            {},
                            {QStringLiteral("Ctrl+A"),
                             QStringLiteral("Ctrl+B")});
    cleanupAction(a.id);

    EXPECT_EQ(a.getKeySequence(0), QKeySequence(QStringLiteral("Ctrl+A")));
    EXPECT_EQ(a.getKeySequence(1), QKeySequence(QStringLiteral("Ctrl+B")));
    EXPECT_EQ(a.getKeySequence(5), QKeySequence()); // out of range -> empty

    EXPECT_EQ(a.getDefaultShortcut(0), QStringLiteral("Ctrl+A"));
    EXPECT_EQ(a.getDefaultShortcut(5), QString());

    cleanupAction(a.id);
}

TEST(ActionTest, MouseBindingsRoundTrip)
{
    Action a
        = Action::make(QStringLiteral("test_action_mouse"), QStringLiteral("Test"));
    cleanupAction(a.id);

    EXPECT_TRUE(a.get_mouse_bindings().isEmpty());

    Binding b;
    b.mouseButton = QStringLiteral("Middle");
    b.mouseModifiers = {QStringLiteral("Ctrl")};
    a.setMouseBindings({b});

    const QList<Binding> stored = a.get_mouse_bindings();
    ASSERT_EQ(stored.size(), 1);
    EXPECT_EQ(stored.first().mouseButton, QStringLiteral("Middle"));

    cleanupAction(a.id);
}

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
    SettingsHandler::get_instance()->remove_json_value(QStringLiteral("Actions"),
                                                    id);
    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("Actions"), id + QStringLiteral("_mouse"));
}
} // namespace

TEST(ActionTest, DisplayTextStripsMnemonicButKeepsEscapedAmpersand)
{
    Action a = Action::make(QStringLiteral("dt_a"),
                            QStringLiteral("&Open Recent"));
    EXPECT_EQ(a.display_text(), QStringLiteral("Open Recent"));

    Action b = Action::make(QStringLiteral("dt_b"),
                            QStringLiteral("Save && Close"));
    EXPECT_EQ(b.display_text(), QStringLiteral("Save & Close"));
}

TEST(ActionTest, GetShortcutsFallsBackToDefaultThenPersistsOverride)
{
    Action a = Action::make(QStringLiteral("test_action_shortcuts"),
                            QStringLiteral("Test"),
                            {},
                            {QStringLiteral("Ctrl+T")});
    cleanupAction(a.id());

    EXPECT_EQ(a.get_shortcuts(), QStringList{QStringLiteral("Ctrl+T")});
    EXPECT_FALSE(a.shortcuts_changed());

    a.set_shortcuts({QStringLiteral("Ctrl+Shift+T")});
    EXPECT_EQ(a.get_shortcuts(),
             QStringList{QStringLiteral("Ctrl+Shift+T")});
    EXPECT_TRUE(a.shortcuts_changed());

    cleanupAction(a.id());
}

TEST(ActionTest, GetKeySequenceAndDefaultShortcutAreIndexBased)
{
    Action a = Action::make(QStringLiteral("test_action_idx"),
                            QStringLiteral("Test"),
                            {},
                            {QStringLiteral("Ctrl+A"),
                             QStringLiteral("Ctrl+B")});
    cleanupAction(a.id());

    EXPECT_EQ(a.get_key_sequence(0), QKeySequence(QStringLiteral("Ctrl+A")));
    EXPECT_EQ(a.get_key_sequence(1), QKeySequence(QStringLiteral("Ctrl+B")));
    EXPECT_EQ(a.get_key_sequence(5), QKeySequence()); // out of range -> empty

    EXPECT_EQ(a.get_default_shortcut(0), QStringLiteral("Ctrl+A"));
    EXPECT_EQ(a.get_default_shortcut(5), QString());

    cleanupAction(a.id());
}

TEST(ActionTest, MouseBindingsRoundTrip)
{
    Action a
        = Action::make(QStringLiteral("test_action_mouse"), QStringLiteral("Test"));
    cleanupAction(a.id());

    EXPECT_TRUE(a.get_mouse_bindings().isEmpty());

    Binding b;
    b.set_mouse_button(QStringLiteral("Middle"));
    b.set_mouse_modifiers({QStringLiteral("Ctrl")});
    a.set_mouse_bindings({b});

    const QList<Binding> stored = a.get_mouse_bindings();
    ASSERT_EQ(stored.size(), 1);
    EXPECT_EQ(stored.first().mouse_button(), QStringLiteral("Middle"));

    cleanupAction(a.id());
}

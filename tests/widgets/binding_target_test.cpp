#include "widgets/controls/binding_target.h"

#include "actions/actions.h"
#include "core/settingshandler.h"

#include <gtest/gtest.h>

namespace {
void cleanupAction(const QString& id)
{
    SettingsHandler::get_instance()->remove_json_value(QStringLiteral("Actions"),
                                                    id);
    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("Actions"), id + QStringLiteral("_mouse"));
}
} // namespace

TEST(ActionBindingTargetTest, WrapsActionIdTextAndBindings)
{
    Action action = Action::make(QStringLiteral("bt_test_action"),
                                 QStringLiteral("Open Recent..."),
                                 {},
                                 {QStringLiteral("Ctrl+R")});
    cleanupAction(action.id());

    ActionBindingTarget target(&action);

    EXPECT_EQ(target.id(), QStringLiteral("bt_test_action"));
    // Trailing "..." stripped for display (ActionBindingTarget::text()) -
    // "Open Recent", not "Open Recent...".
    EXPECT_EQ(target.text(), QStringLiteral("Open Recent"));
    EXPECT_EQ(target.kind(), BindingTargetKind::Action);
    EXPECT_FALSE(target.is_invertible());

    ASSERT_EQ(target.bindings().size(), 1);
    EXPECT_EQ(target.bindings().first().key_sequence(), QStringLiteral("Ctrl+R"));
    EXPECT_FALSE(target.bindings_changed());

    cleanupAction(action.id());
}

TEST(ActionBindingTargetTest, SetBindingsSplitsKeyboardAndMouseAliases)
{
    Action action
        = Action::make(QStringLiteral("bt_test_action2"), QStringLiteral("Test"));
    cleanupAction(action.id());

    ActionBindingTarget target(&action);

    Binding keyOnly;
    keyOnly.set_key_sequence(QStringLiteral("Ctrl+K"));
    Binding mouseOnly;
    mouseOnly.set_mouse_button(QStringLiteral("Middle"));

    target.set_bindings({keyOnly, mouseOnly});

    EXPECT_EQ(action.get_shortcuts(), QStringList{QStringLiteral("Ctrl+K")});
    ASSERT_EQ(action.get_mouse_bindings().size(), 1);
    EXPECT_EQ(action.get_mouse_bindings().first().mouse_button(),
             QStringLiteral("Middle"));

    cleanupAction(action.id());
}

TEST(MouseConfigBindingTargetTest, DelegatesToWrappedMouseConfig)
{
    const MouseConfig& zoom = KeyboardSettings::mouse_actions()[0]; // "zoom"
    MouseConfigBindingTarget target(&zoom, BindingTargetKind::MouseControl);

    EXPECT_EQ(target.id(), zoom.id());
    EXPECT_EQ(target.text(), zoom.text());
    EXPECT_EQ(target.kind(), BindingTargetKind::MouseControl);
    EXPECT_EQ(target.is_invertible(), zoom.is_invertible());
    EXPECT_EQ(target.default_bindings(), zoom.default_bindings());
}

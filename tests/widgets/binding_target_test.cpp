#include "widgets/controls/binding_target.h"

#include "actions/actions.h"
#include "core/settingshandler.h"

#include <gtest/gtest.h>

namespace {
void cleanupAction(const QString& id)
{
    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                    id);
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), id + QStringLiteral("_mouse"));
}
} // namespace

TEST(ActionBindingTargetTest, WrapsActionIdTextAndBindings)
{
    Action action = Action::make(QStringLiteral("bt_test_action"),
                                 QStringLiteral("Open Recent..."),
                                 {},
                                 {QStringLiteral("Ctrl+R")});
    cleanupAction(action.id);

    ActionBindingTarget target(&action);

    EXPECT_EQ(target.id(), QStringLiteral("bt_test_action"));
    // Trailing "..." stripped for display (ActionBindingTarget::text()) -
    // "Open Recent", not "Open Recent...".
    EXPECT_EQ(target.text(), QStringLiteral("Open Recent"));
    EXPECT_EQ(target.kind(), BindingTargetKind::Action);
    EXPECT_FALSE(target.isInvertible());

    ASSERT_EQ(target.bindings().size(), 1);
    EXPECT_EQ(target.bindings().first().keySequence, QStringLiteral("Ctrl+R"));
    EXPECT_FALSE(target.bindingsChanged());

    cleanupAction(action.id);
}

TEST(ActionBindingTargetTest, SetBindingsSplitsKeyboardAndMouseAliases)
{
    Action action
        = Action::make(QStringLiteral("bt_test_action2"), QStringLiteral("Test"));
    cleanupAction(action.id);

    ActionBindingTarget target(&action);

    Binding keyOnly;
    keyOnly.keySequence = QStringLiteral("Ctrl+K");
    Binding mouseOnly;
    mouseOnly.mouseButton = QStringLiteral("Middle");

    target.setBindings({keyOnly, mouseOnly});

    EXPECT_EQ(action.get_shortcuts(), QStringList{QStringLiteral("Ctrl+K")});
    ASSERT_EQ(action.get_mouse_bindings().size(), 1);
    EXPECT_EQ(action.get_mouse_bindings().first().mouseButton,
             QStringLiteral("Middle"));

    cleanupAction(action.id);
}

TEST(MouseConfigBindingTargetTest, DelegatesToWrappedMouseConfig)
{
    const MouseConfig& zoom = KeyboardSettings::mouseActions()[0]; // "zoom"
    MouseConfigBindingTarget target(&zoom, BindingTargetKind::MouseControl);

    EXPECT_EQ(target.id(), zoom.id());
    EXPECT_EQ(target.text(), zoom.text());
    EXPECT_EQ(target.kind(), BindingTargetKind::MouseControl);
    EXPECT_EQ(target.isInvertible(), zoom.isInvertible());
    EXPECT_EQ(target.defaultBindings(), zoom.defaultBindings());
}

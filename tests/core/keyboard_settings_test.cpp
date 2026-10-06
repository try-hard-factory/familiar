#include "core/controls.h"
#include "core/settingshandler.h"

#include <gtest/gtest.h>

#include <QMouseEvent>
#include <QWheelEvent>

// KeyboardSettings (like FamSettings, core/settings.h) is a thin
// value-typed facade over SettingsHandler's singleton storage, already
// redirected to a throwaway temp file by
// tests/support/settings_test_environment.h - safe to mutate here.
// mouseActions()/mousewheelActions() below are the SAME static default
// lists real dispatch code matches against (action_mouse_dispatch.h),
// not test doubles - so a test here failing after someone edits those
// defaults is a real, intended signal.

TEST(KeyboardSettingsListTest, SetListRemovesStorageWhenEqualToDefault)
{
    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("TestGroup"), QStringLiteral("test_list_key"));
    KeyboardSettings ks;
    const QStringList defaults = {QStringLiteral("x")};

    ks.set_list(QStringLiteral("TestGroup"),
              QStringLiteral("test_list_key"),
              {QStringLiteral("a"), QStringLiteral("b")},
              defaults);
    EXPECT_EQ(ks.get_list(QStringLiteral("TestGroup"),
                        QStringLiteral("test_list_key"),
                        defaults),
             (QStringList{QStringLiteral("a"), QStringLiteral("b")}));

    // Writing back the default value removes the stored override
    // entirely (setList()'s own "only non-default data is stored"
    // contract) - getList() then falls back to `defaults` again, not to
    // whatever the JSON document happens to still hold.
    ks.set_list(QStringLiteral("TestGroup"),
              QStringLiteral("test_list_key"),
              defaults,
              defaults);
    EXPECT_EQ(ks.get_list(QStringLiteral("TestGroup"),
                        QStringLiteral("test_list_key"),
                        defaults),
             defaults);
}

TEST(KeyboardSettingsScalarTest, SetScalarRemovesStorageWhenEqualToDefault)
{
    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("TestGroup"), QStringLiteral("test_scalar_key"));
    KeyboardSettings ks;

    ks.set_scalar(QStringLiteral("TestGroup"),
                QStringLiteral("test_scalar_key"),
                42,
                7);
    EXPECT_EQ(ks.get_scalar(QStringLiteral("TestGroup"),
                          QStringLiteral("test_scalar_key"),
                          7)
                 .toInt(),
             42);

    ks.set_scalar(QStringLiteral("TestGroup"),
                QStringLiteral("test_scalar_key"),
                7,
                7);
    EXPECT_EQ(ks.get_scalar(QStringLiteral("TestGroup"),
                          QStringLiteral("test_scalar_key"),
                          7)
                 .toInt(),
             7);
}

TEST(KeyboardSettingsShortcutsTest, GetShortcutsPersistsDefaultOnFirstRead)
{
    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("TestActions"), QStringLiteral("test_action"));
    KeyboardSettings ks;
    ks.set_save_unknown_shortcuts(true);

    const QStringList first = ks.get_shortcuts(
        QStringLiteral("TestActions"),
        QStringLiteral("test_action"),
        {QStringLiteral("Ctrl+K")});
    EXPECT_EQ(first, (QStringList{QStringLiteral("Ctrl+K")}));

    // A DIFFERENT default passed on the second call - if the first call
    // hadn't actually persisted anything, this would come back as
    // "Ctrl+Z" instead of the still-stored "Ctrl+K".
    const QStringList second = ks.get_shortcuts(
        QStringLiteral("TestActions"),
        QStringLiteral("test_action"),
        {QStringLiteral("Ctrl+Z")});
    EXPECT_EQ(second, (QStringList{QStringLiteral("Ctrl+K")}));

    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("TestActions"), QStringLiteral("test_action"));
}

TEST(KeyboardSettingsShortcutsTest, SaveUnknownShortcutsFalseDoesNotPersist)
{
    SettingsHandler::get_instance()->remove_json_value(
        QStringLiteral("TestActions"), QStringLiteral("test_action2"));
    KeyboardSettings ks;
    ks.set_save_unknown_shortcuts(false);

    ks.get_shortcuts(QStringLiteral("TestActions"),
                     QStringLiteral("test_action2"),
                     {QStringLiteral("Ctrl+K")});

    EXPECT_TRUE(SettingsHandler::get_instance()
                   ->json_value(QStringLiteral("TestActions"),
                               QStringLiteral("test_action2"))
                   .isUndefined());
}

// ─── Conflict detection ─────────────────────────────────────────────────

TEST(FindConflictingMouseGroupTest, DetectsButtonAndModifierMatch)
{
    // "Controls" is the shared JSON group both MouseConfig and
    // MouseWheelConfig store into (see MouseConfig::settingsGroup()'s own
    // comment) - clearing it means getBindings() falls back to each
    // action's hardcoded defaultBindings_, so this test isn't order-
    // dependent on whatever an earlier TEST() may have stored.
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Controls"));
    KeyboardSettings ks;

    // "zoom"'s own default binding (Middle + Ctrl, see
    // KeyboardSettings::mouseActions()).
    Binding candidate;
    candidate.set_mouse_button(QStringLiteral("Middle"));
    candidate.set_mouse_modifiers({QStringLiteral("Ctrl")});

    EXPECT_EQ(ks.find_conflicting_mouse_group(QStringLiteral("pan"), candidate),
             0); // index of "zoom"
    EXPECT_EQ(ks.find_conflicting_mouse_group(QStringLiteral("zoom"), candidate),
             -1); // excluding zoom itself, nothing else uses Middle+Ctrl
}

TEST(FindConflictingMouseGroupTest, EmptyCandidateNeverConflicts)
{
    KeyboardSettings ks;
    Binding empty;
    EXPECT_EQ(ks.find_conflicting_mouse_group(QStringLiteral("zoom"), empty), -1);
}

TEST(FindConflictingWheelGroupTest, DetectsModifierMatch)
{
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Controls"));
    KeyboardSettings ks;

    // "pan_horizontal"'s own default binding (Shift alone, see
    // KeyboardSettings::mousewheelActions()).
    Binding candidate;
    candidate.set_mouse_modifiers({QStringLiteral("Shift")});

    EXPECT_EQ(ks.find_conflicting_wheel_group(QStringLiteral("pan_vertical"),
                                          candidate),
             0); // index of "pan_horizontal"
    EXPECT_EQ(ks.find_conflicting_wheel_group(QStringLiteral("pan_horizontal"),
                                          candidate),
             -1);
}

// ─── matchesEvent ────────────────────────────────────────────────────────

TEST(MouseConfigMatchesEventTest, MatchesButtonAndModifiers)
{
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Controls"));
    const MouseConfig& pan = KeyboardSettings::mouse_actions()[1]; // "pan": Left + Alt

    QMouseEvent matching(QEvent::MouseButtonPress,
                        QPointF(0, 0),
                        QPointF(0, 0),
                        Qt::LeftButton,
                        Qt::LeftButton,
                        Qt::AltModifier);
    EXPECT_TRUE(pan.matches_event(&matching).has_value());

    QMouseEvent wrongModifier(QEvent::MouseButtonPress,
                             QPointF(0, 0),
                             QPointF(0, 0),
                             Qt::LeftButton,
                             Qt::LeftButton,
                             Qt::NoModifier);
    EXPECT_FALSE(pan.matches_event(&wrongModifier).has_value());
}

TEST(MouseWheelConfigMatchesEventTest, MatchesModifierAndReturnsInverted)
{
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Controls"));
    // "pan_horizontal": Shift alone, inverted = true.
    const MouseWheelConfig& panH = KeyboardSettings::mousewheel_actions()[0];

    QWheelEvent matching(QPointF(0, 0),
                        QPointF(0, 0),
                        QPoint(0, 0),
                        QPoint(0, 120),
                        Qt::NoButton,
                        Qt::ShiftModifier,
                        Qt::NoScrollPhase,
                        false);
    const auto match = panH.matches_event(&matching);
    ASSERT_TRUE(match.has_value());
    EXPECT_TRUE(match->is_inverted());

    QWheelEvent wrongModifier(QPointF(0, 0),
                             QPointF(0, 0),
                             QPoint(0, 0),
                             QPoint(0, 120),
                             Qt::NoButton,
                             Qt::NoModifier,
                             Qt::NoScrollPhase,
                             false);
    EXPECT_FALSE(panH.matches_event(&wrongModifier).has_value());
}

TEST(KeyboardSettingsEventDispatchTest, MouseActionForEventFindsMatchingGroup)
{
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Controls"));
    KeyboardSettings ks;

    QMouseEvent press(QEvent::MouseButtonPress,
                     QPointF(0, 0),
                     QPointF(0, 0),
                     Qt::LeftButton,
                     Qt::LeftButton,
                     Qt::AltModifier);
    const auto match = ks.mouse_action_for_event(&press);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->group, QStringLiteral("pan"));
}

// ─── MouseConfig::setButton / getButton round trip ─────────────────────

TEST(MouseConfigTest, SetButtonPersistsAndGetButtonReflectsIt)
{
    const MouseConfig& zoom = KeyboardSettings::mouse_actions()[0]; // "zoom"
    const QList<Binding> original = zoom.get_bindings();

    zoom.set_button(QStringLiteral("Right"));
    EXPECT_EQ(zoom.get_button(), QStringLiteral("Right"));
    EXPECT_TRUE(zoom.is_configured());

    zoom.set_button(QStringLiteral("Not Configured"));
    EXPECT_EQ(zoom.get_button(), QStringLiteral("Not Configured"));
    EXPECT_FALSE(zoom.is_configured());

    zoom.set_bindings(original); // restore, shared storage across TESTs
}

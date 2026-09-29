#include "core/controls.h"

#include <gtest/gtest.h>

#include <QKeyEvent>
#include <QKeySequence>

// ─── Binding ────────────────────────────────────────────────────────────

TEST(BindingTest, SerializeDeserializeRoundTrips)
{
    Binding b;
    b.keySequence = QStringLiteral("Ctrl+S");
    b.mouseButton = QStringLiteral("Left");
    b.mouseModifiers = {QStringLiteral("Ctrl"), QStringLiteral("Alt")};
    b.inverted = true;
    b.systemGlobal = false;

    EXPECT_EQ(Binding::deserialize(b.serialize()), b);
}

TEST(BindingTest, EmptyBindingRoundTrips)
{
    Binding b;
    EXPECT_EQ(Binding::deserialize(b.serialize()), b);
}

TEST(BindingTest, ClassificationHelpers)
{
    Binding empty;
    EXPECT_TRUE(empty.is_empty());
    EXPECT_FALSE(empty.is_keyboard_only());
    EXPECT_FALSE(empty.is_mouse_only());
    EXPECT_FALSE(empty.is_mixed());

    Binding keyOnly;
    keyOnly.keySequence = QStringLiteral("Ctrl+S");
    EXPECT_FALSE(keyOnly.is_empty());
    EXPECT_TRUE(keyOnly.is_keyboard_only());
    EXPECT_FALSE(keyOnly.is_mouse_only());
    EXPECT_FALSE(keyOnly.is_mixed());

    Binding mouseOnly;
    mouseOnly.mouseButton = QStringLiteral("Left");
    EXPECT_FALSE(mouseOnly.is_keyboard_only());
    EXPECT_TRUE(mouseOnly.is_mouse_only());

    Binding mixed;
    mixed.keySequence = QStringLiteral("F");
    mixed.mouseButton = QStringLiteral("Middle");
    EXPECT_TRUE(mixed.is_mixed());
    EXPECT_FALSE(mixed.is_keyboard_only());
    EXPECT_FALSE(mixed.is_mouse_only());
}

TEST(BindingTest, DisplayTextCombinesPartsWithPlusSeparator)
{
    Binding keyOnly;
    keyOnly.keySequence = QStringLiteral("Ctrl+S");
    EXPECT_EQ(keyOnly.display_text(), QStringLiteral("Ctrl+S"));

    Binding mouseWithModifiers;
    mouseWithModifiers.mouseButton = QStringLiteral("Left");
    mouseWithModifiers.mouseModifiers = {QStringLiteral("Ctrl"),
                                         QStringLiteral("Alt")};
    EXPECT_EQ(mouseWithModifiers.display_text(),
              QStringLiteral("Left MB + Ctrl+Alt"));

    // Wheel binding: no button/key, but a real (non-"No Modifier")
    // modifier requirement - still needs some text so it doesn't read as
    // "not configured".
    Binding wheelWithModifierOnly;
    wheelWithModifierOnly.mouseModifiers = {QStringLiteral("Ctrl")};
    EXPECT_EQ(wheelWithModifierOnly.display_text(), QStringLiteral("Ctrl"));

    // "No Modifier" alone (scroll with nothing held) is spelled out
    // explicitly instead of coming out blank - see Binding::displayText()'s
    // own comment for why.
    Binding wheelNoModifier;
    wheelNoModifier.mouseModifiers = {QStringLiteral("No Modifier")};
    EXPECT_EQ(wheelNoModifier.display_text(), QStringLiteral("No Modifier"));
}

// ─── MouseConfigBase::modifiersToQt ────────────────────────────────────

TEST(ModifiersToQtTest, CombinesFlagsFromNames)
{
    const Qt::KeyboardModifiers result = MouseConfigBase::modifiers_to_qt(
        {QStringLiteral("Ctrl"), QStringLiteral("Shift")});
    EXPECT_EQ(result, Qt::ControlModifier | Qt::ShiftModifier);
}

TEST(ModifiersToQtTest, UnknownNameIsIgnored)
{
    EXPECT_EQ(MouseConfigBase::modifiers_to_qt({QStringLiteral("Bogus")}),
              Qt::NoModifier);
}

TEST(ModifiersToQtTest, EmptyListIsNoModifier)
{
    EXPECT_EQ(MouseConfigBase::modifiers_to_qt({}), Qt::NoModifier);
}

// ─── keyEventToSequenceString ──────────────────────────────────────────

TEST(KeyEventToSequenceStringTest, BareModifierPressReturnsPlainName)
{
    // QKeySequence has no representation for "modifier alone, no key" -
    // special-cased in keyEventToSequenceString() itself (core/controls.cpp)
    // rather than falling through to QKeySequence::toString().
    QKeyEvent ctrlOnly(QEvent::KeyPress, Qt::Key_Control, Qt::NoModifier);
    EXPECT_EQ(key_event_to_sequence_string(&ctrlOnly), QStringLiteral("Ctrl"));

    QKeyEvent altOnly(QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    EXPECT_EQ(key_event_to_sequence_string(&altOnly), QStringLiteral("Alt"));
}

TEST(KeyEventToSequenceStringTest, NormalKeyUsesQKeySequenceFormat)
{
    // Expected value computed via the same QKeySequence(...).toString()
    // call the function itself makes, not a hardcoded string - this
    // checks keyEventToSequenceString()'s own logic (does it fall
    // through to QKeySequence correctly for a non-bare-modifier key),
    // not whatever platform-specific text QKeySequence happens to
    // render ("Ctrl+S" vs "⌘S", ...).
    QKeyEvent ctrlS(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    const QString expected = QKeySequence(ctrlS.keyCombination()).toString();
    EXPECT_EQ(key_event_to_sequence_string(&ctrlS), expected);
}

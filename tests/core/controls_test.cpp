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
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_FALSE(empty.isKeyboardOnly());
    EXPECT_FALSE(empty.isMouseOnly());
    EXPECT_FALSE(empty.isMixed());

    Binding keyOnly;
    keyOnly.keySequence = QStringLiteral("Ctrl+S");
    EXPECT_FALSE(keyOnly.isEmpty());
    EXPECT_TRUE(keyOnly.isKeyboardOnly());
    EXPECT_FALSE(keyOnly.isMouseOnly());
    EXPECT_FALSE(keyOnly.isMixed());

    Binding mouseOnly;
    mouseOnly.mouseButton = QStringLiteral("Left");
    EXPECT_FALSE(mouseOnly.isKeyboardOnly());
    EXPECT_TRUE(mouseOnly.isMouseOnly());

    Binding mixed;
    mixed.keySequence = QStringLiteral("F");
    mixed.mouseButton = QStringLiteral("Middle");
    EXPECT_TRUE(mixed.isMixed());
    EXPECT_FALSE(mixed.isKeyboardOnly());
    EXPECT_FALSE(mixed.isMouseOnly());
}

TEST(BindingTest, DisplayTextCombinesPartsWithPlusSeparator)
{
    Binding keyOnly;
    keyOnly.keySequence = QStringLiteral("Ctrl+S");
    EXPECT_EQ(keyOnly.displayText(), QStringLiteral("Ctrl+S"));

    Binding mouseWithModifiers;
    mouseWithModifiers.mouseButton = QStringLiteral("Left");
    mouseWithModifiers.mouseModifiers
        = {QStringLiteral("Ctrl"), QStringLiteral("Alt")};
    EXPECT_EQ(mouseWithModifiers.displayText(),
             QStringLiteral("Left MB + Ctrl+Alt"));

    // Wheel binding: no button/key, but a real (non-"No Modifier")
    // modifier requirement - still needs some text so it doesn't read as
    // "not configured".
    Binding wheelWithModifierOnly;
    wheelWithModifierOnly.mouseModifiers = {QStringLiteral("Ctrl")};
    EXPECT_EQ(wheelWithModifierOnly.displayText(), QStringLiteral("Ctrl"));

    // "No Modifier" alone (scroll with nothing held) is spelled out
    // explicitly instead of coming out blank - see Binding::displayText()'s
    // own comment for why.
    Binding wheelNoModifier;
    wheelNoModifier.mouseModifiers = {QStringLiteral("No Modifier")};
    EXPECT_EQ(wheelNoModifier.displayText(), QStringLiteral("No Modifier"));
}

// ─── MouseConfigBase::modifiersToQt ────────────────────────────────────

TEST(ModifiersToQtTest, CombinesFlagsFromNames)
{
    const Qt::KeyboardModifiers result = MouseConfigBase::modifiersToQt(
        {QStringLiteral("Ctrl"), QStringLiteral("Shift")});
    EXPECT_EQ(result, Qt::ControlModifier | Qt::ShiftModifier);
}

TEST(ModifiersToQtTest, UnknownNameIsIgnored)
{
    EXPECT_EQ(MouseConfigBase::modifiersToQt({QStringLiteral("Bogus")}),
             Qt::NoModifier);
}

TEST(ModifiersToQtTest, EmptyListIsNoModifier)
{
    EXPECT_EQ(MouseConfigBase::modifiersToQt({}), Qt::NoModifier);
}

// ─── keyEventToSequenceString ──────────────────────────────────────────

TEST(KeyEventToSequenceStringTest, BareModifierPressReturnsPlainName)
{
    // QKeySequence has no representation for "modifier alone, no key" -
    // special-cased in keyEventToSequenceString() itself (core/controls.cpp)
    // rather than falling through to QKeySequence::toString().
    QKeyEvent ctrlOnly(QEvent::KeyPress, Qt::Key_Control, Qt::NoModifier);
    EXPECT_EQ(keyEventToSequenceString(&ctrlOnly), QStringLiteral("Ctrl"));

    QKeyEvent altOnly(QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    EXPECT_EQ(keyEventToSequenceString(&altOnly), QStringLiteral("Alt"));
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
    const QString expected
        = QKeySequence(ctrlS.keyCombination()).toString();
    EXPECT_EQ(keyEventToSequenceString(&ctrlS), expected);
}

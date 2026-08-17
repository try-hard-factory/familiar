#include "widgets/controls/binding_dialogs.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QKeyEvent>
#include <QTest>

// BindingEditorDialogBase/AddAliasDialog/RebindDialog aren't exercised
// here - they need a real BindingTarget (Action* or MouseConfigBase*)
// and pull in conflict-detection + showMessageBox - the two capture
// fields below are the self-contained, no-CanvasScene piece of this
// file. show()+qWaitForWindowExposed() before sending events - see
// tests/widgets/flat_checkbox_test.cpp's own comment for why.

TEST(MouseButtonCaptureFieldTest, ClickingRecordsWhicheverButtonWasUsed)
{
    MouseButtonCaptureField field;
    field.resize(150, 24);
    field.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&field));

    EXPECT_TRUE(field.button().isEmpty());

    QTest::mousePress(&field, Qt::MiddleButton);
    EXPECT_EQ(field.button(), QStringLiteral("Middle"));
    EXPECT_EQ(field.text(), QStringLiteral("Middle MB"));

    QTest::mousePress(&field, Qt::RightButton);
    EXPECT_EQ(field.button(), QStringLiteral("Right"));
}

TEST(MouseButtonCaptureFieldTest, SetButtonAndClearBindingUpdateDisplay)
{
    MouseButtonCaptureField field;

    field.setButton(QStringLiteral("Left"));
    EXPECT_EQ(field.button(), QStringLiteral("Left"));
    EXPECT_EQ(field.text(), QStringLiteral("Left MB"));

    field.clearBinding();
    EXPECT_TRUE(field.button().isEmpty());
    EXPECT_TRUE(field.text().isEmpty());
}

TEST(KeySequenceCaptureFieldTest, KeyPressRecordsSequenceViaSharedHelper)
{
    KeySequenceCaptureField field;
    field.resize(150, 24);
    field.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&field));

    EXPECT_TRUE(field.sequence().isEmpty());

    QTest::keyClick(&field, Qt::Key_S, Qt::ControlModifier);

    // keyEventToSequenceString() (core/controls.cpp) already has its own
    // direct unit tests in tests/core/controls_test.cpp - what matters
    // here is that this widget actually wires keyPressEvent() to it and
    // reflects the result, not re-deriving the exact expected string.
    EXPECT_FALSE(field.sequence().isEmpty());
    EXPECT_EQ(field.text(), field.sequence());
}

TEST(KeySequenceCaptureFieldTest, EscapeIsIgnoredNotCaptured)
{
    // Manual QKeyEvent + sendEvent, not QTest::keyClick - need to inspect
    // isAccepted() afterward (QTest::keyClick's own event isn't
    // reachable), which is how keyPressEvent()'s "let the dialog's own
    // Escape-to-close handle it instead" contract (this class's own
    // comment) is actually verifiable: event->ignore(), not just "the
    // field's own state didn't change".
    KeySequenceCaptureField field;
    field.setSequence(QStringLiteral("Ctrl+S")); // pre-existing binding

    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&field, &escape);

    EXPECT_FALSE(escape.isAccepted());
    EXPECT_EQ(field.sequence(), QStringLiteral("Ctrl+S")); // untouched
}

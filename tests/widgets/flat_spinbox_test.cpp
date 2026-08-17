#include "widgets/flat_spinbox.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTest>

// FlatSpinBox only overrides paintEvent()/resizeEvent()/enterEvent()/
// leaveEvent() (widgets/flat_spinbox.h's own comment) - the up/down
// step behavior tested here is QAbstractSpinBox's own, completely
// unmodified. This is a regression check that the hand-painted chrome
// didn't disturb it, via real keyboard events rather than calling
// stepUp()/stepDown() directly (which would bypass QAbstractSpinBox's
// own keyPressEvent() entirely and not actually prove anything about
// the real widget).
//
// show() + qWaitForWindowExposed() before sending events - see
// tests/widgets/flat_checkbox_test.cpp's own comment: without a real
// exposed window, QAbstractButton/QAbstractSpinBox's press/key handling
// silently no-ops (confirmed via testing on both a real X11 session and
// QT_QPA_PLATFORM=offscreen).

TEST(FlatSpinBoxTest, ArrowKeysStepValueAndEmitValueChanged)
{
    // Brace-init, not FlatSpinBox box(QColor(Qt::white), ...) - MSVC
    // misparses that as a function DECLARATION named `box` (three
    // consecutive same-type functional-cast arguments trip its most-
    // vexing-parse handling, even though Qt::white being a qualified
    // name should rule that reading out per the standard - confirmed via
    // a real CI failure: "error C2751: 'Qt::white': the name of a
    // function parameter cannot be qualified"). GCC/Clang never saw the
    // ambiguity. Braces are never ambiguous with a declaration on any
    // compiler.
    FlatSpinBox box{QColor(Qt::white), QColor(Qt::black), QColor(Qt::lightGray)};
    box.setRange(0, 10);
    box.setValue(5);
    box.resize(80, 24);
    box.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&box));

    QSignalSpy valueChangedSpy(&box, QOverload<int>::of(&QSpinBox::valueChanged));

    QTest::keyClick(&box, Qt::Key_Up);
    EXPECT_EQ(box.value(), 6);

    QTest::keyClick(&box, Qt::Key_Down);
    QTest::keyClick(&box, Qt::Key_Down);
    EXPECT_EQ(box.value(), 4);

    EXPECT_EQ(valueChangedSpy.count(), 3);
}

TEST(FlatSpinBoxTest, ClampsToRange)
{
    // Brace-init - see the other TEST() above for why plain parens break
    // on MSVC here.
    FlatSpinBox box{QColor(Qt::white), QColor(Qt::black), QColor(Qt::lightGray)};
    box.setRange(0, 3);
    box.setValue(3);
    box.resize(80, 24);
    box.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&box));

    QTest::keyClick(&box, Qt::Key_Up); // already at the maximum
    EXPECT_EQ(box.value(), 3);

    box.setValue(0);
    QTest::keyClick(&box, Qt::Key_Down); // already at the minimum
    EXPECT_EQ(box.value(), 0);
}

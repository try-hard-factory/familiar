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
    FlatSpinBox box(QColor(Qt::white), QColor(Qt::black), QColor(Qt::lightGray));
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
    FlatSpinBox box(QColor(Qt::white), QColor(Qt::black), QColor(Qt::lightGray));
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

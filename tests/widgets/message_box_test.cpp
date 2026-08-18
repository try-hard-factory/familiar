#include "widgets/message_box.h"

#include <gtest/gtest.h>

#include <QPushButton>
#include <QTest>

// exec() isn't called anywhere here (blocking modal) - done() (via a
// button click) works fine called indirectly without an active event
// loop. reject() itself is protected (CustomMessageBox overrides it to
// resolve to escapeButton_ instead of a plain Rejected) - not callable
// directly from a test, so it's exercised the same way a real user
// triggers it: QDialog's own keyPressEvent() resolves Escape to
// reject(), which needs show()+qWaitForWindowExposed() first, same
// reasoning as every other synthetic-event test in this suite (see
// tests/widgets/flat_checkbox_test.cpp's own comment).

TEST(CustomMessageBoxTest, ClickingAButtonSetsDialogResult)
{
    CustomMessageBox box(QMessageBox::Warning,
                         nullptr,
                         QStringLiteral("Title"),
                         QStringLiteral("Text"),
                         QMessageBox::Yes | QMessageBox::No,
                         QMessageBox::Yes);
    box.resize(340, 150);
    box.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&box));

    QPushButton* noBtn = nullptr;
    for (QPushButton* b : box.findChildren<QPushButton*>()) {
        if (b->text() == QObject::tr("No")) {
            noBtn = b;
        }
    }
    ASSERT_NE(noBtn, nullptr);

    QTest::mouseClick(noBtn, Qt::LeftButton);
    EXPECT_EQ(box.result(), int(QMessageBox::No));
}

TEST(CustomMessageBoxTest, EscapeButtonPrefersCancelOverNoOverDefault)
{
    {
        CustomMessageBox box(QMessageBox::Warning,
                             nullptr,
                             {},
                             {},
                             QMessageBox::Cancel | QMessageBox::No
                                 | QMessageBox::Ok,
                             QMessageBox::Ok);
        box.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&box));
        QTest::keyClick(&box, Qt::Key_Escape);
        EXPECT_EQ(box.result(), int(QMessageBox::Cancel));
    }
    {
        CustomMessageBox box(QMessageBox::Warning,
                             nullptr,
                             {},
                             {},
                             QMessageBox::No | QMessageBox::Ok,
                             QMessageBox::Ok);
        box.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&box));
        QTest::keyClick(&box, Qt::Key_Escape);
        EXPECT_EQ(box.result(), int(QMessageBox::No));
    }
    {
        // Neither Cancel nor No present - falls back to the caller's own
        // defaultButton (CustomMessageBox's own constructor comment).
        CustomMessageBox box(QMessageBox::Warning,
                             nullptr,
                             {},
                             {},
                             QMessageBox::Ok,
                             QMessageBox::Ok);
        box.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&box));
        QTest::keyClick(&box, Qt::Key_Escape);
        EXPECT_EQ(box.result(), int(QMessageBox::Ok));
    }
}

TEST(CustomMessageBoxTest, PrimaryButtonIsDefaultAndRenderedLast)
{
    CustomMessageBox box(QMessageBox::Warning,
                         nullptr,
                         {},
                         {},
                         QMessageBox::Yes | QMessageBox::No,
                         QMessageBox::Yes);

    QList<QPushButton*> actionButtons;
    for (QPushButton* b : box.findChildren<QPushButton*>()) {
        // Excludes the "x" corner close glyph (objectName "cmbCloseBtn") -
        // not one of the Yes/No action buttons under test here.
        if (b->objectName() != QStringLiteral("cmbCloseBtn")) {
            actionButtons.append(b);
        }
    }

    ASSERT_EQ(actionButtons.size(), 2);
    // Primary (Yes, the caller's defaultButton) is added last - rightmost/
    // most prominent, see CustomMessageBox's own constructor comment.
    EXPECT_TRUE(actionButtons.last()->isDefault());
    EXPECT_EQ(actionButtons.last()->text(), QObject::tr("Yes"));
}

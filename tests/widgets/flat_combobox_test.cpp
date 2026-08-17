#include "widgets/flat_combobox.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>

// showPopup() itself is deliberately not exercised here - it opens a
// real top-level popup window and defers mask/style work via
// QTimer::singleShot (widgets/flat_combobox.cpp), which would need an
// event-loop spin and a real window to tear down cleanly. What's tested
// instead is that hand-painting FlatComboBox's closed-box chrome didn't
// disturb the underlying QComboBox's own index/selection machinery, plus
// FlatComboItemDelegate's one piece of real logic (the minimum row
// height floor).

TEST(FlatComboBoxTest, SelectingItemUpdatesCurrentTextAndEmitsSignal)
{
    // Brace-init, not parens - MSVC misparses 3+ consecutive same-type
    // functional-cast constructor args (QColor(Qt::x), QColor(Qt::y), ...)
    // as a function DECLARATION instead of a call - see
    // tests/widgets/flat_spinbox_test.cpp's own comment for the real CI
    // error this produces there. Braces sidestep the ambiguity entirely.
    FlatComboBox box{QColor(Qt::white),
                     QColor(Qt::black),
                     QColor(Qt::lightGray),
                     QColor(Qt::blue)};
    box.addItem(QStringLiteral("First"));
    box.addItem(QStringLiteral("Second"));
    box.addItem(QStringLiteral("Third"));

    QSignalSpy indexChangedSpy(&box,
                              QOverload<int>::of(
                                  &QComboBox::currentIndexChanged));

    box.setCurrentIndex(2);

    EXPECT_EQ(box.currentIndex(), 2);
    EXPECT_EQ(box.currentText(), QStringLiteral("Third"));
    ASSERT_EQ(indexChangedSpy.count(), 1);
    EXPECT_EQ(indexChangedSpy.takeFirst().at(0).toInt(), 2);
}

TEST(FlatComboItemDelegateTest, SizeHintEnforcesMinimumRowHeight)
{
    // Brace-init - same MSVC most-vexing-parse risk as FlatComboBox's own
    // constructor above.
    FlatComboItemDelegate delegate{QColor(Qt::black), QColor(Qt::lightGray)};

    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("x")));

    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 100, 10);

    // The floor (std::max(s.height(), 32) in
    // FlatComboItemDelegate::sizeHint()) holds regardless of what the
    // base QStyledItemDelegate::sizeHint() itself returns for this
    // style/font - that's the whole point of the override.
    const QSize hint = delegate.sizeHint(option, model.index(0, 0));
    EXPECT_GE(hint.height(), 32);
}

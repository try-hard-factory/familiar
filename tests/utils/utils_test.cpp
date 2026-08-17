#include "utils/utils.h"

#include <gtest/gtest.h>

#include <QWidget>

// centered_widget<M, S>() is only ever called with QWidget-derived types
// (MainWindow*, dialog subclasses - see help_dialog.cpp, about_dialog.cpp,
// mainwindow.cpp, ...), all of which share plain QWidget's
// mapToGlobal()/size()/move() for this purpose. Exercised here with bare
// QWidgets rather than one of those concrete types, since the template
// itself doesn't care which - and it's the arithmetic being tested, not
// any dialog-specific behavior.
//
// `expected` below is computed via the SAME parent.mapToGlobal(QPoint(0,
// 0)) call centered_widget() itself makes, not a hardcoded screen
// coordinate - keeps this a check of centered_widget()'s offset math,
// not an assertion about what mapToGlobal() happens to return on
// whatever windowing system this runs under (neither widget is ever
// shown, so no window manager round-trip is involved either way).
TEST(CenteredWidgetTest, CentersWidgetOverParent)
{
    QWidget parent;
    parent.move(50, 50);
    parent.resize(200, 100);

    QWidget child;
    child.resize(50, 40);

    centered_widget(&parent, &child);

    const QPoint expected = parent.mapToGlobal(QPoint(0, 0))
                            + QPoint((200 - 50) / 2, (100 - 40) / 2);
    EXPECT_EQ(child.pos(), expected);
}

TEST(GetRectFromPointsTest, NormalizesRegardlessOfPointOrder)
{
    // Bottom-right-to-top-left drag (point1 "after" point2 on both axes)
    // - the rect it produces should be identical to the top-left-to-
    // bottom-right drag a user more usually makes.
    const QRectF fromReverseDrag
        = get_rect_from_points(QPointF(100, 80), QPointF(10, 20));
    const QRectF fromForwardDrag
        = get_rect_from_points(QPointF(10, 20), QPointF(100, 80));

    EXPECT_EQ(fromReverseDrag, fromForwardDrag);
    EXPECT_EQ(fromForwardDrag, QRectF(QPointF(10, 20), QPointF(100, 80)));
}

TEST(GetRectFromPointsTest, ZeroSizeWhenPointsCoincide)
{
    const QRectF r = get_rect_from_points(QPointF(5, 5), QPointF(5, 5));
    EXPECT_TRUE(r.isEmpty());
}

TEST(RoundToTest, RoundsToNearestMultiple)
{
    EXPECT_DOUBLE_EQ(roundTo(23.0, 10.0), 20.0);
    EXPECT_DOUBLE_EQ(roundTo(27.0, 10.0), 30.0);
    EXPECT_DOUBLE_EQ(roundTo(0.0, 10.0), 0.0);
}

TEST(RoundToTest, NegativeNumbers)
{
    EXPECT_DOUBLE_EQ(roundTo(-23.0, 10.0), -20.0);
}

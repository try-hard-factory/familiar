#include "core/qguiappcurrentscreen.h"

#include <gtest/gtest.h>

#include <QGuiApplication>
#include <QScreen>

// The macOS-only edge-correction branch (screenAt() nullptr near the
// bottom/right edge - core/qguiappcurrentscreen.cpp's own comment) isn't
// exercised here - #if defined(Q_OS_MACOS), not reachable on this
// platform. currentScreen() (no-arg, QCursor::pos()-based) isn't tested
// either - moving the real system cursor from a test isn't something
// this suite controls deterministically across platforms/CI.

TEST(QGuiAppCurrentScreenTest, ReturnsScreenContainingPosition)
{
    QScreen* primary = qGuiApp->primaryScreen();
    ASSERT_NE(primary, nullptr);

    QGuiAppCurrentScreen current;
    QScreen* found = current.currentScreen(primary->geometry().center());
    EXPECT_EQ(found, primary);
}

TEST(QGuiAppCurrentScreenTest,
    FallsBackToPrimaryScreenWhenPositionIsOffAllScreens)
{
    QScreen* primary = qGuiApp->primaryScreen();
    ASSERT_NE(primary, nullptr);

    QGuiAppCurrentScreen current;
    // Far outside any real screen's geometry - screenAt() returns
    // nullptr, so this exercises the qGuiApp->primaryScreen() fallback
    // (core/qguiappcurrentscreen.cpp's own qCritical()-logged branch).
    QScreen* found = current.currentScreen(QPoint(1000000, 1000000));
    EXPECT_EQ(found, primary);
}

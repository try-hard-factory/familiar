#include "widgets/settings_style.h"

#include <gtest/gtest.h>

using namespace familiar::settings_style;

// Same substring-containment approach as dialog_style_test.cpp - these
// builders take no parameters (everything comes from the fixed
// palette()), so what's worth locking down is "does the output actually
// use palette()'s real colors", not the exact QSS text.

TEST(SettingsStyleTest, PaletteIsAFixedSingleton)
{
    const Palette& p = palette();
    EXPECT_TRUE(p.background.isValid());
    EXPECT_TRUE(p.text.isValid());
    // settings_style.h's own comment: this window always renders in one
    // scheme regardless of the user's chosen app-wide accent preset -
    // palette() has no parameter to select a different one.
    EXPECT_EQ(&palette(), &palette());
}

TEST(SettingsStyleTest, RootStyleSheetIncludesBackgroundAndTextColors)
{
    const Palette& p = palette();
    const QString qss = rootStyleSheet();
    EXPECT_TRUE(qss.contains(p.background.name()));
    EXPECT_TRUE(qss.contains(p.text.name()));
}

TEST(SettingsStyleTest, SidebarButtonStyleSheetIncludesNavColors)
{
    const Palette& p = palette();
    const QString qss = sidebarButtonStyleSheet();
    EXPECT_TRUE(qss.contains(p.navIdleBg.name()));
    EXPECT_TRUE(qss.contains(p.navSelectedBg.name()));
    EXPECT_TRUE(qss.contains(p.navHoverBg.name()));
}

TEST(SettingsStyleTest, ShortcutChipStyleSheetIncludesChipColors)
{
    const Palette& p = palette();
    const QString qss = shortcutChipStyleSheet();
    EXPECT_TRUE(qss.contains(p.chipBackground.name()));
    EXPECT_TRUE(qss.contains(p.border.name()));
}

TEST(SettingsStyleTest, FilledButtonStyleSheetIncludesNavColors)
{
    const Palette& p = palette();
    const QString qss = filledButtonStyleSheet();
    EXPECT_TRUE(qss.contains(p.navIdleBg.name()));
    EXPECT_TRUE(qss.contains(p.navHoverBg.name()));
    EXPECT_TRUE(qss.contains(p.navSelectedBg.name()));
}

TEST(SettingsStyleTest, SliderStyleSheetUsesNavSelectedBgNotAccent)
{
    // Deliberately gray (navSelectedBg), not accent/orange - see
    // sliderStyleSheet()'s own comment: the orange read as an error/
    // warning color on this page, so it was swapped for the same
    // neutral gray this window's other "on" states already use.
    const Palette& p = palette();
    const QString qss = sliderStyleSheet();
    EXPECT_TRUE(qss.contains(p.navSelectedBg.name()));
    EXPECT_FALSE(qss.contains(p.accent.name()));
}

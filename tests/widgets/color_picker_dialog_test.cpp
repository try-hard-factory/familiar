#include "widgets/color_picker_dialog.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTest>

// SvPicker/HueSlider/AlphaSlider/SwatchRow all convert a mouse position
// into a color value via a private pick_()/swatchAt_() helper only
// reachable through mousePressEvent() - exercised here via real
// QTest::mousePress() at known coordinates against an explicitly resized
// widget, checking the resulting signal (none of the four expose a
// public getter for their own current value - by design, the signal IS
// the observable contract). show()+qWaitForWindowExposed() first - see
// tests/widgets/flat_checkbox_test.cpp's own comment for why.
//
// ColorPickerDialog itself (the QDialog wiring these four together) and
// showColorPickerDialog() aren't exercised here - modal exec(), not
// worth automating just to re-check wiring the pieces below already
// cover individually.

TEST(SvPickerTest, MousePressComputesSaturationAndValueFromPosition)
{
    SvPicker picker;
    // SvPicker::SvPicker() calls setMinimumWidth(220) - resizing below
    // that is silently clamped back up to 220 by Qt, so 220 (not some
    // arbitrary smaller width) is what a resize() call actually needs to
    // request for the math below to hold. Height is setFixedHeight(170)
    // in the same constructor, so 170 always holds regardless.
    picker.resize(220, 170);
    picker.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&picker));

    QSignalSpy spy(&picker, &SvPicker::svChanged);
    // Half width, full height (bottom) - s = 0.5, v = 1 - 1.0 = 0.
    QTest::mousePress(&picker, Qt::LeftButton, {}, QPoint(110, 170));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.first().at(0).toDouble(), 0.5);
    EXPECT_DOUBLE_EQ(spy.first().at(1).toDouble(), 0.0);
}

TEST(SvPickerTest, ClampsOutOfBoundsPositions)
{
    SvPicker picker;
    picker.resize(220, 170);
    picker.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&picker));

    QSignalSpy spy(&picker, &SvPicker::svChanged);
    QTest::mousePress(&picker, Qt::LeftButton, {}, QPoint(-50, -50));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.first().at(0).toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(spy.first().at(1).toDouble(), 1.0);
}

TEST(HueSliderTest, MousePressComputesHueFromXPositionAndClamps)
{
    HueSlider slider;
    slider.resize(360, 18);
    slider.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&slider));

    QSignalSpy spy(&slider, &HueSlider::hueChanged);

    QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(180, 9));
    EXPECT_EQ(slider.hue(), int(180 / 360.0 * 359));

    QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(-10, 9));
    EXPECT_EQ(slider.hue(), 0);

    QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(10000, 9));
    EXPECT_EQ(slider.hue(), 359);

    EXPECT_EQ(spy.count(), 3);
}

TEST(AlphaSliderTest, MousePressComputesAlphaFromXPositionAndClamps)
{
    AlphaSlider slider;
    slider.resize(255, 18);
    slider.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&slider));

    QSignalSpy spy(&slider, &AlphaSlider::alphaChanged);

    QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(128, 9));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toInt(), 128);

    QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(-10, 9));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toInt(), 0);

    QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(10000, 9));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toInt(), 255);
}

TEST(SwatchRowTest, ClickingASwatchEmitsItsColor)
{
    // 8 fixed presets (withNone=false) - see SwatchRow's own constructor
    // - cellW = width/8, chosen as 40 so the math has no rounding.
    SwatchRow row{/*withNone=*/false, QColor(Qt::blue)};
    row.resize(320, row.height());
    row.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&row));

    QSignalSpy spy(&row, &SwatchRow::swatchPicked);
    QTest::mousePress(&row, Qt::LeftButton, {}, QPoint(20, row.height() / 2));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().at(0).value<QColor>(), QColor(0x1a, 0x1a, 0x1a));
}

TEST(SwatchRowTest, ClickingTheGapBetweenSwatchesEmitsNothing)
{
    SwatchRow row{false, QColor(Qt::blue)};
    row.resize(320, row.height());
    row.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&row));

    QSignalSpy spy(&row, &SwatchRow::swatchPicked);
    // cellRect_(0) is a centered 26px square inside a 40px cell (see
    // cellRect_()'s own comment) - x=35 falls in the surrounding gap.
    QTest::mousePress(&row, Qt::LeftButton, {}, QPoint(35, row.height() / 2));

    EXPECT_EQ(spy.count(), 0);
}

TEST(SwatchRowTest, WithNonePrependsTransparentSwatch)
{
    SwatchRow row{/*withNone=*/true, QColor(Qt::blue)}; // 9 swatches now
    row.resize(360, row.height());
    row.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&row));

    QSignalSpy spy(&row, &SwatchRow::swatchPicked);
    QTest::mousePress(&row, Qt::LeftButton, {}, QPoint(20, row.height() / 2));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().at(0).value<QColor>(), QColor(0, 0, 0, 0));
}

#include "widgets/color_picker_dialog.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTest>
#include <QLineEdit>

namespace {

QLineEdit* hex_field(ColorPickerDialog& dialog)
{
    for (auto* field : dialog.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("RRGGBB")) {
            return field;
        }
    }
    return nullptr;
}

} // namespace

// SvPicker/HueSlider/AlphaSlider/SwatchRow all convert a mouse position
// into a color value via a private pick_()/swatchAt_() helper only
// reachable through mousePressEvent() - exercised here via real
// QTest::mousePress() at known coordinates against an explicitly resized
// widget, checking the resulting signal (none of the four expose a
// public getter for their own current value - by design, the signal IS
// the observable contract). show()+qWaitForWindowExposed() first - see
// tests/widgets/flat_checkbox_test.cpp's own comment for why.
//
// The dialog's field synchronization can be exercised without opening
// its modal event loop; show_color_picker_dialog() itself remains manual.

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

    QSignalSpy spy(&picker, &SvPicker::sv_changed);
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

    QSignalSpy spy(&picker, &SvPicker::sv_changed);
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

    QSignalSpy spy(&slider, &HueSlider::hue_changed);

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

    QSignalSpy spy(&slider, &AlphaSlider::alpha_changed);

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

    QSignalSpy spy(&row, &SwatchRow::swatch_picked);
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

    QSignalSpy spy(&row, &SwatchRow::swatch_picked);
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

    QSignalSpy spy(&row, &SwatchRow::swatch_picked);
    QTest::mousePress(&row, Qt::LeftButton, {}, QPoint(20, row.height() / 2));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().at(0).value<QColor>(), QColor(0, 0, 0, 0));
}

TEST(ColorPickerDialogTest, HexEditChangesRgbWithoutDiscardingAlpha)
{
    ColorPickerDialog dialog(nullptr, QColor(20, 30, 40, 80),
                             QStringLiteral("Color"));
    auto* hex = hex_field(dialog);
    ASSERT_NE(hex, nullptr);
    QSignalSpy changed(&dialog, &ColorPickerDialog::color_changed);

    hex->setText(QStringLiteral("AABBCC"));
    ASSERT_TRUE(QMetaObject::invokeMethod(hex, "editingFinished"));

    EXPECT_EQ(dialog.selected_color(), QColor(0xAA, 0xBB, 0xCC, 80));
    EXPECT_EQ(changed.count(), 1);
}

TEST(ColorPickerDialogTest, InvalidHexEditLeavesCurrentColorAlone)
{
    const QColor initial(20, 30, 40, 80);
    ColorPickerDialog dialog(nullptr, initial, QStringLiteral("Color"));
    auto* hex = hex_field(dialog);
    ASSERT_NE(hex, nullptr);
    QSignalSpy changed(&dialog, &ColorPickerDialog::color_changed);

    hex->setText(QStringLiteral("no-color"));
    ASSERT_TRUE(QMetaObject::invokeMethod(hex, "editingFinished"));

    EXPECT_EQ(dialog.selected_color(), initial);
    EXPECT_EQ(changed.count(), 0);
}

TEST(ColorPickerDialogTest, OpacityPercentageIsClampedToFullAlpha)
{
    ColorPickerDialog dialog(nullptr, QColor(20, 30, 40, 80),
                             QStringLiteral("Color"));
    QLineEdit* percentage = nullptr;
    for (auto* field : dialog.findChildren<QLineEdit*>()) {
        if (field != hex_field(dialog)) {
            percentage = field;
        }
    }
    ASSERT_NE(percentage, nullptr);

    percentage->setText(QStringLiteral("120"));
    ASSERT_TRUE(QMetaObject::invokeMethod(percentage, "editingFinished"));
    EXPECT_EQ(dialog.selected_color().alpha(), 255);

    percentage->setText(QStringLiteral("invalid"));
    ASSERT_TRUE(QMetaObject::invokeMethod(percentage, "editingFinished"));
    EXPECT_EQ(dialog.selected_color().alpha(), 255);
}

TEST(ColorPickerDialogTest, NoAlphaModeAlwaysStartsOpaque)
{
    ColorPickerDialog dialog(nullptr, QColor(20, 30, 40, 80),
                             QStringLiteral("Color"), /*withAlpha=*/false);

    EXPECT_EQ(dialog.selected_color().alpha(), 255);
    EXPECT_EQ(dialog.findChildren<AlphaSlider*>().size(), 0);
}

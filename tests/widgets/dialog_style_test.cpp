#include "widgets/dialog_style.h"

#include <gtest/gtest.h>

#include <QPushButton>
#include <QWidget>

using namespace familiar::dialog_style;

// QSS-builder functions - checked by substring containment (does the
// output actually incorporate the given class/object name and colors),
// not exact string matching - keeps these tests from breaking on every
// unrelated formatting tweak to the QSS while still catching a real
// regression like an accidentally swapped/dropped argument.

TEST(DialogStyleTest, PanelStyleSheetIncludesClassNameAndColors)
{
    const QColor background(0x11, 0x22, 0x33);
    const QColor border(0x44, 0x55, 0x66);
    const QColor text(0x77, 0x88, 0x99);

    const QString qss
        = panelStyleSheet("MyDialog", background, border, text);

    EXPECT_TRUE(qss.contains(QStringLiteral("MyDialog")));
    EXPECT_TRUE(qss.contains(background.name()));
    EXPECT_TRUE(qss.contains(border.name()));
    // Default radius (unspecified 5th arg) - the usual rounded-panel
    // look every custom dialog gets unless it opts out (CustomMessageBox
    // does, via radiusPx=0 - see dialog_style.h's own comment).
    EXPECT_TRUE(qss.contains(QStringLiteral("border-radius: 10px")));
}

TEST(DialogStyleTest, PanelStyleSheetRadiusPxOverridesTheDefault)
{
    const QString qss = panelStyleSheet("MyDialog",
                                        QColor(Qt::black),
                                        QColor(Qt::black),
                                        QColor(Qt::black),
                                        /*radiusPx=*/0);

    EXPECT_TRUE(qss.contains(QStringLiteral("border-radius: 0px")));
    EXPECT_FALSE(qss.contains(QStringLiteral("border-radius: 10px")));
}

TEST(DialogStyleTest, CloseButtonStyleSheetIncludesObjectNameAndTextColor)
{
    const QColor text(0x11, 0x22, 0x33);
    const QColor accent(0x44, 0x55, 0x66);

    const QString qss = closeButtonStyleSheet("closeBtn", text, accent);

    EXPECT_TRUE(qss.contains(QStringLiteral("closeBtn")));
    EXPECT_TRUE(qss.contains(text.name()));
}

TEST(DialogStyleTest, StylePrimaryButtonAppliesAccentColor)
{
    QPushButton button;
    const QColor accent(0xAA, 0xBB, 0xCC);
    stylePrimaryButton(&button, accent);
    EXPECT_TRUE(button.styleSheet().contains(accent.name()));
}

TEST(DialogStyleTest, StyleSecondaryButtonAppliesTextAndBorderColors)
{
    QPushButton button;
    const QColor text(0x11, 0x22, 0x33);
    const QColor border(0x44, 0x55, 0x66);
    styleSecondaryButton(&button, text, border);
    EXPECT_TRUE(button.styleSheet().contains(text.name()));
    EXPECT_TRUE(button.styleSheet().contains(border.name()));
}

TEST(DialogStyleTest, SeverityIconScalesWithDevicePixelRatio)
{
    const QPixmap icon1x
        = severityIcon(QMessageBox::Warning, QColor(Qt::blue), 1.0);
    const QPixmap icon2x
        = severityIcon(QMessageBox::Warning, QColor(Qt::blue), 2.0);

    ASSERT_FALSE(icon1x.isNull());
    ASSERT_FALSE(icon2x.isNull());
    // Physical pixel size (QPixmap::width()/height(), not device-
    // independent) scales linearly with dpr - see severityIcon()'s own
    // QPixmap(QSize(kIconSize, kIconSize) * dpr) construction.
    EXPECT_EQ(icon2x.width(), icon1x.width() * 2);
    EXPECT_EQ(icon2x.height(), icon1x.height() * 2);
}

TEST(DialogStyleTest, ApplyRoundedMaskSetsNonEmptyMask)
{
    QWidget widget;
    widget.resize(100, 60);
    applyRoundedMask(&widget, 8);
    EXPECT_FALSE(widget.mask().isEmpty());
}

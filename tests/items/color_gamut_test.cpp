#include "moveitem.h"
#include "widgets/color_gamut.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QObject>

TEST(ColorGamutTest, CountsVisibleChromaticPixelsByHueAndSaturation)
{
    QImage image(3, 2, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    image.setPixelColor(0, 0, Qt::red);
    image.setPixelColor(1, 0, Qt::red);
    image.setPixelColor(2, 0, Qt::green);
    image.setPixelColor(0, 1, Qt::white);
    image.setPixelColor(1, 1, Qt::black);

    const auto gamut = PixmapItem::color_gamut_from_image(image);

    EXPECT_EQ(gamut.size(), 2);
    EXPECT_EQ(gamut.value(qMakePair(0, 255)), 2);
    EXPECT_EQ(gamut.value(qMakePair(120, 255)), 1);
}

TEST(ColorGamutTest, CancellationDiscardsPartialHistogram)
{
    QImage image(3, 1, QImage::Format_RGB32);
    image.fill(Qt::red);
    int visitedColumns = 0;

    const auto gamut = PixmapItem::color_gamut_from_image(image, [&] {
        return ++visitedColumns == 2;
    });

    EXPECT_EQ(visitedColumns, 2);
    EXPECT_TRUE(gamut.isEmpty());
}

TEST(ColorGamutPainterTest, ThresholdControlsWhichSamplesAreDrawn)
{
    PixmapItem::ColorGamut gamut;
    gamut[qMakePair(0, 128)] = 25;
    GamutPainterThread painter(nullptr, QImage(), gamut);
    QImage rendered;
    QObject::connect(&painter, &GamutPainterThread::image_ready,
                     [&rendered](const QImage& image) { rendered = image; });

    painter.set_threshold(30);
    painter.run();
    ASSERT_EQ(rendered.size(), QSize(500, 500));
    EXPECT_EQ(rendered.pixelColor(125, 250), QColor(Qt::black));

    painter.set_threshold(20);
    painter.run();
    EXPECT_GT(rendered.pixelColor(125, 250).red(),
              rendered.pixelColor(125, 250).green());
    ASSERT_TRUE(painter.gamut().has_value());
    EXPECT_EQ(painter.gamut()->value(qMakePair(0, 128)), 25);
}

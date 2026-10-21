#include "moveitem.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QPixmap>
#include <QRandomGenerator>

// Regression tests for the generational re-encoding bug: every save ran
// pixmap_to_bytes() over the DECODED pixmap, so a picture stored as JPEG
// (what "best" picks for anything over kSmallImageSide without alpha,
// an originally-PNG import included) was re-compressed on every
// save->load->save round, autosave and crash-recovery snapshots
// included. See PixmapItem's encodedCache_ for the measurements.

namespace {

// Noise, deliberately: it is the worst case for JPEG, so generational
// loss shows up as a large error instead of a rounding difference.
QImage noise_image(int width, int height)
{
    QImage img(width, height, QImage::Format_RGB32);
    QRandomGenerator gen(1234U);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            img.setPixel(x,
                         y,
                         qRgb(gen.bounded(256),
                              gen.bounded(256),
                              gen.bounded(256)));
        }
    }
    return img;
}

double mean_channel_error(const QImage& a, const QImage& b)
{
    if (a.size() != b.size()) {
        return -1.0;
    }
    double sum = 0.0;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            const QColor ca = a.pixelColor(x, y);
            const QColor cb = b.pixelColor(x, y);
            sum += std::abs(ca.red() - cb.red())
                   + std::abs(ca.green() - cb.green())
                   + std::abs(ca.blue() - cb.blue());
        }
    }
    return sum / (double(a.width()) * a.height() * 3.0);
}

} // namespace

// 800x600 with no alpha channel: over kSmallImageSide, so "best" sends it
// to JPEG - the exact case that degraded.
TEST(PixmapItemEncodingTest, RepeatedSavesReturnIdenticalBytes)
{
    PixmapItem item(noise_image(800, 600));

    const auto [first, firstFormat] = item.pixmap_to_bytes();
    const auto [second, secondFormat] = item.pixmap_to_bytes();
    const auto [third, thirdFormat] = item.pixmap_to_bytes();

    EXPECT_FALSE(first.isEmpty());
    EXPECT_EQ(first, second);
    EXPECT_EQ(first, third);
    EXPECT_EQ(firstFormat, secondFormat);
    EXPECT_EQ(firstFormat, thirdFormat);
}

// What FmlArchive::load() + add_queued_items() do: hand the item the very
// bytes its pixmap was decoded from. The next save must write them back
// untouched, whatever the storage-format setting would have chosen.
TEST(PixmapItemEncodingTest, SeededSourceIsReturnedVerbatim)
{
    const QImage source = noise_image(800, 600);
    QByteArray png;
    {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        ASSERT_TRUE(source.save(&buffer, "PNG"));
    }

    PixmapItem item(source);
    item.set_encoded_source(png, QStringLiteral("png"));

    const auto [bytes, format] = item.pixmap_to_bytes();
    EXPECT_EQ(bytes, png);
    EXPECT_EQ(format, QStringLiteral("png"));
}

// The whole point: the error must stop growing after the first encode.
TEST(PixmapItemEncodingTest, ErrorDoesNotGrowAcrossGenerations)
{
    const QImage original = noise_image(800, 600);

    // Generation 1 - the one unavoidable encode.
    PixmapItem first(original);
    auto [bytes, format] = first.pixmap_to_bytes();
    QImage decoded;
    ASSERT_TRUE(decoded.loadFromData(bytes));
    const double firstError = mean_channel_error(original, decoded);
    ASSERT_GT(firstError, 0.0) << "noise must actually lose something to JPEG";

    // Generations 2..5: load, seed from the stored bytes, save again.
    QByteArray carried = bytes;
    for (int generation = 2; generation <= 5; ++generation) {
        QImage loaded;
        ASSERT_TRUE(loaded.loadFromData(carried));
        PixmapItem item(loaded);
        item.set_encoded_source(carried, format);

        const auto [again, againFormat] = item.pixmap_to_bytes();
        EXPECT_EQ(again, carried) << "generation " << generation
                                  << " re-encoded instead of storing as-is";
        EXPECT_EQ(againFormat, format);

        QImage roundTripped;
        ASSERT_TRUE(roundTripped.loadFromData(again));
        EXPECT_DOUBLE_EQ(mean_channel_error(original, roundTripped), firstError)
            << "error grew at generation " << generation;
        carried = again;
    }
}

// Replacing the pixels must drop the cache - the key is the pixmap's own
// cacheKey(), so this needs no explicit invalidation anywhere.
TEST(PixmapItemEncodingTest, NewPixmapInvalidatesCache)
{
    PixmapItem item(noise_image(800, 600));
    const auto [before, _] = item.pixmap_to_bytes();

    item.setPixmap(QPixmap::fromImage(noise_image(640, 480)));
    const auto [after, afterFormat] = item.pixmap_to_bytes();

    EXPECT_FALSE(after.isEmpty());
    EXPECT_NE(before, after);
    EXPECT_FALSE(afterFormat.isEmpty());
}

// The export path asks for a grayscale/cropped rendering, which is a
// different image than the stored one and must never come out of the
// cache (export.cpp).
TEST(PixmapItemEncodingTest, GrayscaleExportIsNotServedFromCache)
{
    PixmapItem item(noise_image(800, 600));
    const auto [stored, _] = item.pixmap_to_bytes();

    item.set_grayscale(true);
    const auto [gray, grayFormat] = item.pixmap_to_bytes(/*applyGrayscale=*/true);

    EXPECT_FALSE(gray.isEmpty());
    EXPECT_NE(stored, gray);
    EXPECT_FALSE(grayFormat.isEmpty());

    // ...and asking again without grayscale still returns the stored bytes.
    const auto [storedAgain, __] = item.pixmap_to_bytes();
    EXPECT_EQ(stored, storedAgain);
}


// sample_color_at() used to read the raw pixmap wherever itemAt() handed
// it a point - and shape() (selector.h) claims a margin beyond the
// visible edge, so the eyedropper reported colours from pixels the crop
// hides, or coordinates outside the image entirely (an invalid colour
// plus a qWarning per mouse move, routed into this app's own log).
TEST(PixmapItemColorSamplingTest, OnlySamplesInsideTheVisibleCrop)
{
    QImage img(200, 200, QImage::Format_RGB32);
    img.fill(Qt::red);
    // The band that a crop will hide, painted differently so sampling it
    // by mistake is unmistakable.
    for (int y = 0; y < 200; ++y) {
        for (int x = 150; x < 200; ++x) {
            img.setPixel(x, y, qRgb(0, 0, 255));
        }
    }

    PixmapItem item(img);
    item.set_crop(QRectF(0, 0, 150, 200)); // the blue band is cropped away

    // Inside the crop: the colour that is really there.
    EXPECT_EQ(item.sample_color_at(QPointF(10, 10)).name(),
              QColor(Qt::red).name());

    // Inside the pixmap but cropped away - what the margin around the
    // visible edge used to reach. Must report nothing rather than blue.
    EXPECT_FALSE(item.sample_color_at(QPointF(170, 100)).isValid());

    // Outside the image altogether.
    EXPECT_FALSE(item.sample_color_at(QPointF(500, 500)).isValid());
    EXPECT_FALSE(item.sample_color_at(QPointF(-5, -5)).isValid());
}

// A crop wider than the pixmap (nothing validates what a .fml carries)
// must not get past the bounds check either.
TEST(PixmapItemColorSamplingTest, ACropLargerThanTheImageIsStillClamped)
{
    QImage img(100, 100, QImage::Format_RGB32);
    img.fill(Qt::green);

    PixmapItem item(img);
    item.set_crop(QRectF(0, 0, 10000, 10000));

    EXPECT_EQ(item.sample_color_at(QPointF(50, 50)).name(),
              QColor(Qt::green).name());
    EXPECT_FALSE(item.sample_color_at(QPointF(500, 500)).isValid());
}

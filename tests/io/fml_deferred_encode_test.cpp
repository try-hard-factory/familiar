#include "canvasscene.h"
#include "fml_archive.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QImage>
#include <QRectF>
#include <QTemporaryDir>
#include <QUndoStack>

#include <memory>

// FmlArchive::capture() is the half that reads the scene, and it used to
// encode every picture's pixels on the way through - on the GUI thread,
// because that is the only thread allowed to touch an item. A picture
// whose bytes were already cached (PixmapItem::encodedCache_) cost
// nothing, but one that had never been encoded cost a full encode:
// measured 2364 ms for 80 800x800 pictures, against 2 ms once their
// bytes were cached. The recovery snapshot pays that on the first tick
// after new pictures arrive.
//
// capture(Encode::Deferred) now hands the pixels over instead and
// write() encodes them, which is already allowed to run on a worker
// thread. adopt_encoded_images() is what keeps that a one-time cost: it
// gives the bytes back to the pictures afterwards.

namespace {

class FmlDeferredEncodeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
        ASSERT_TRUE(dir_.isValid());
    }

    // Noise, so that an encode is real work and its output is not
    // accidentally identical to anything else.
    static QImage noise_image(int side, quint32 seed)
    {
        QImage img(side, side, QImage::Format_RGB32);
        quint32 state = 0x2545F491 + seed;
        for (int y = 0; y < side; ++y) {
            auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < side; ++x) {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                line[x] = qRgb(state & 0xFF,
                               (state >> 8) & 0xFF,
                               (state >> 16) & 0xFF);
            }
        }
        return img;
    }

    // A picture nothing has ever encoded, which is the case this is all
    // about - one loaded from a .fml or imported arrives with its bytes
    // already cached.
    PixmapItem* add_fresh_picture(quint32 seed = 0)
    {
        auto* item = new PixmapItem(noise_image(kside, seed));
        scene_->addItem(item);
        return item;
    }

    QString path(const QString& name) const { return dir_.filePath(name); }

    static constexpr int kside = 200;

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
    QTemporaryDir dir_;
};

} // namespace

TEST_F(FmlDeferredEncodeTest, CaptureEncodesOnTheSpotByDefault)
{
    PixmapItem* picture = add_fresh_picture();

    FmlSnapshot snapshot;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(), QRectF(), snapshot));

    ASSERT_EQ(snapshot.images.size(), 1);
    EXPECT_FALSE(snapshot.images.first().source.encoded.isEmpty());
    // And on the item too, which is what makes every later save cheap.
    EXPECT_FALSE(picture->encoded_source().isEmpty());
}

TEST_F(FmlDeferredEncodeTest, CaptureLeavesTheEncodingToWriteWhenAsked)
{
    PixmapItem* picture = add_fresh_picture();

    FmlSnapshot snapshot;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(),
                                    QRectF(),
                                    snapshot,
                                    nullptr,
                                    FmlArchive::Encode::Deferred));

    ASSERT_EQ(snapshot.images.size(), 1);
    const FmlSnapshot::Image& image = snapshot.images.first();
    EXPECT_TRUE(image.source.encoded.isEmpty());
    // The pixels went along instead, and the format is settled already
    // because the archive entry is named after it.
    EXPECT_FALSE(image.source.image.isNull());
    EXPECT_FALSE(image.source.format.isEmpty());
    EXPECT_TRUE(image.archiveName.endsWith(image.source.format));
    // Nothing was written to the item either.
    EXPECT_TRUE(picture->encoded_source().isEmpty());

    const FmlResult result = FmlArchive::write(snapshot, path("p.fml"));

    ASSERT_TRUE(result.error.isEmpty());
    EXPECT_FALSE(snapshot.images.first().source.encoded.isEmpty());
}

// The archive must not depend on which half did the encoding.
TEST_F(FmlDeferredEncodeTest, BothHalvesProduceTheSameBytes)
{
    add_fresh_picture();

    FmlSnapshot deferred;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(),
                                    QRectF(),
                                    deferred,
                                    nullptr,
                                    FmlArchive::Encode::Deferred));
    ASSERT_TRUE(FmlArchive::write(deferred, path("deferred.fml")).error.isEmpty());

    FmlSnapshot now;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(), QRectF(), now));

    ASSERT_EQ(deferred.images.size(), 1);
    ASSERT_EQ(now.images.size(), 1);
    EXPECT_EQ(deferred.images.first().source.format,
              now.images.first().source.format);
    EXPECT_EQ(deferred.images.first().source.encoded,
              now.images.first().source.encoded);
}

TEST_F(FmlDeferredEncodeTest, TheDeferredBytesAreGivenBackToThePicture)
{
    PixmapItem* picture = add_fresh_picture();

    FmlSnapshot snapshot;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(),
                                    QRectF(),
                                    snapshot,
                                    nullptr,
                                    FmlArchive::Encode::Deferred));
    ASSERT_TRUE(FmlArchive::write(snapshot, path("p.fml")).error.isEmpty());
    ASSERT_TRUE(picture->encoded_source().isEmpty());

    FmlArchive::adopt_encoded_images(scene_.get(), snapshot);

    EXPECT_EQ(picture->encoded_source(), snapshot.images.first().source.encoded);
    EXPECT_EQ(picture->encoded_source_format(),
              snapshot.images.first().source.format);
}

// Every picture is given its own bytes and no one else's.
TEST_F(FmlDeferredEncodeTest, TwoPicturesAreNotMixedUp)
{
    PixmapItem* first = add_fresh_picture(1);
    PixmapItem* second = add_fresh_picture(2);

    FmlSnapshot snapshot;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(),
                                    QRectF(),
                                    snapshot,
                                    nullptr,
                                    FmlArchive::Encode::Deferred));
    ASSERT_TRUE(FmlArchive::write(snapshot, path("p.fml")).error.isEmpty());
    FmlArchive::adopt_encoded_images(scene_.get(), snapshot);

    ASSERT_FALSE(first->encoded_source().isEmpty());
    ASSERT_FALSE(second->encoded_source().isEmpty());
    EXPECT_NE(first->encoded_source(), second->encoded_source());

    // Each one's bytes really do decode back to its own picture.
    QImage decoded;
    ASSERT_TRUE(decoded.loadFromData(first->encoded_source()));
    EXPECT_EQ(decoded, first->pixmap().toImage());
}

// The whole point of the cache key: the encode happens on another
// thread, so the picture may be a different one by the time its bytes
// come back. Those bytes describe what was captured, not what the item
// holds now, and storing them would make the next save write the wrong
// pixels.
TEST_F(FmlDeferredEncodeTest, APictureThatChangedIsNotGivenStaleBytes)
{
    PixmapItem* picture = add_fresh_picture(1);

    FmlSnapshot snapshot;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(),
                                    QRectF(),
                                    snapshot,
                                    nullptr,
                                    FmlArchive::Encode::Deferred));
    ASSERT_TRUE(FmlArchive::write(snapshot, path("p.fml")).error.isEmpty());

    picture->setPixmap(QPixmap::fromImage(noise_image(kside, 99)));
    FmlArchive::adopt_encoded_images(scene_.get(), snapshot);

    EXPECT_TRUE(picture->encoded_source().isEmpty());
}

// A gif's archive entry is the animation's own stream, not an encoding
// of the frame its pixmap happens to be showing - so it must never be
// handed back as that pixmap's bytes.
TEST_F(FmlDeferredEncodeTest, AGifIsNotGivenItsOwnStreamAsAFrame)
{
    auto* gif = new GifItem(
        QByteArray::fromBase64("R0lGODlhBAAEAPAAAAAAAP///yH/C05FVFNDQVBFMi4wAwEAAAAh+QQA"
                               "CgAAACwAAAAABAAEAAACBwQAAAAAACgAIfkEAAoAAAAsAAAAAAQABAAA"
                               "AgdMkiRJkiQpADs="),
        QStringLiteral("anim.gif"));
    scene_->addItem(gif);

    FmlSnapshot snapshot;
    ASSERT_TRUE(FmlArchive::capture(scene_.get(),
                                    QRectF(),
                                    snapshot,
                                    nullptr,
                                    FmlArchive::Encode::Deferred));
    ASSERT_EQ(snapshot.images.size(), 1);
    ASSERT_TRUE(snapshot.images.first().archiveName.endsWith(
        QStringLiteral(".gif")));
    ASSERT_TRUE(FmlArchive::write(snapshot, path("p.fml")).error.isEmpty());

    FmlArchive::adopt_encoded_images(scene_.get(), snapshot);

    EXPECT_TRUE(gif->encoded_source().isEmpty());
}

#include "moveitem.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>
#include <QPixmap>

#include <memory>

// GifItem's constructor used to decode every frame of the animation, at
// full size, to build the 48px filmstrip thumbnails - 1869 ms on a
// 150-frame 1920x1080 GIF, synchronously on the GUI thread, for every
// animated picture imported, pasted or loaded from a .fml. The filmstrip
// starts hidden and only opens on a button press
// (GifPlaybackToolbar::toggle_filmstrip()), so almost none of that work
// was ever looked at.
//
// The thumbnails are built on first request now, and the frame count -
// which stepping and scrubbing DO need from the start - comes from
// QImageReader::imageCount() instead, which answered in 2 ms on that
// same file.

namespace {

// A real two-frame animated GIF, 4x4, black then white. Same literal as
// tests/io/scene_export_test.cpp uses.
QByteArray gif_bytes()
{
    return QByteArray::fromBase64("R0lGODlhBAAEAPAAAAAAAP///yH/C05FVFNDQVBFMi4wAwEAAAAh+QQA"
                                  "CgAAACwAAAAABAAEAAACBwQAAAAAACgAIfkEAAoAAAAsAAAAAAQABAAA"
                                  "AgdMkiRJkiQpADs=");
}

std::unique_ptr<GifItem> make_gif()
{
    return std::make_unique<GifItem>(gif_bytes(), QStringLiteral("anim.gif"));
}

} // namespace

TEST(GifFrameCountTest, TheFrameCountIsKnownWithoutBuildingTheThumbnails)
{
    const std::unique_ptr<GifItem> item = make_gif();

    EXPECT_EQ(item->frame_count(), 2);
    EXPECT_FALSE(item->thumbnails_built());
}

TEST(GifFrameCountTest, TheThumbnailsAreBuiltOnTheFirstRequest)
{
    const std::unique_ptr<GifItem> item = make_gif();

    EXPECT_EQ(item->frame_thumbnails().size(), 2);
    EXPECT_TRUE(item->thumbnails_built());
}

// A QPixmap's cache key is unique to its pixel data, so a second build
// would hand back different ones - which is what the filmstrip would pay
// for on every frame tick while it is open (rebuild_filmstrip() runs
// from on_frame_changed()).
TEST(GifFrameCountTest, TheThumbnailsAreBuiltOnlyOnce)
{
    const std::unique_ptr<GifItem> item = make_gif();

    const qint64 firstKey = item->frame_thumbnails().first().cacheKey();

    EXPECT_EQ(item->frame_thumbnails().first().cacheKey(), firstKey);
    EXPECT_EQ(item->frame_thumbnails().size(), 2);
}

// Both of these used to bail out on an empty frameThumbnails_, which
// with lazy thumbnails would have meant "does nothing until the user
// opens the filmstrip" - the toolbar's step and scrub buttons are
// reachable without ever opening it.
// Which frame the movie sits on right after construction is QMovie's
// business (start() has already advanced past the jumpToFrame(0) the
// constructor asked for), so both of these move RELATIVE to wherever it
// is - and with two frames, one step away is never where it started,
// which is what an early return would have left it at.
TEST(GifFrameCountTest, SteppingWorksBeforeTheThumbnailsExist)
{
    const std::unique_ptr<GifItem> item = make_gif();
    ASSERT_FALSE(item->thumbnails_built());
    const int before = item->current_frame();
    ASSERT_GE(before, 0);

    item->step_frame(1);

    EXPECT_NE(item->current_frame(), before);
    EXPECT_EQ(item->current_frame(), (before + 1) % item->frame_count());
    EXPECT_FALSE(item->thumbnails_built());
}

TEST(GifFrameCountTest, ScrubbingWorksBeforeTheThumbnailsExist)
{
    const std::unique_ptr<GifItem> item = make_gif();
    const int before = item->current_frame();
    ASSERT_GE(before, 0);
    const int target = (before + 1) % item->frame_count();

    item->jump_to_frame(target);

    EXPECT_EQ(item->current_frame(), target);
    EXPECT_FALSE(item->thumbnails_built());
}

// Nothing can be counted and nothing can be decoded, so the count stays
// 0 and the item is simply inert. (That the failed pass is not re-run on
// every request - what the built flag buys over an isEmpty() check - has
// no observable here; TheThumbnailsAreBuiltOnlyOnce above covers the
// decodable case.)
TEST(GifFrameCountTest, AStreamWithNothingToDecodeIsInertNotBroken)
{
    const std::unique_ptr<GifItem> item = std::make_unique<GifItem>(
        QByteArrayLiteral("not an image at all"),
        QStringLiteral("broken.gif"));

    EXPECT_EQ(item->frame_count(), 0);
    EXPECT_TRUE(item->thumbnails_built());
    EXPECT_TRUE(item->frame_thumbnails().isEmpty());

    // And neither of these does anything rather than dividing by a zero
    // frame count. QMovie answers -1 for "no current frame" here, which
    // is why this compares against what it said before rather than
    // against 0.
    const int before = item->current_frame();
    item->step_frame(1);
    item->jump_to_frame(3);
    EXPECT_EQ(item->current_frame(), before);
}

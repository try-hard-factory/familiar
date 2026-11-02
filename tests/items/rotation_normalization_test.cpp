#include "moveitem.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QPointF>

// set_rotation() stored whatever std::fmod() gave it, and fmod keeps the
// sign of its dividend - while the angle comes from get_rotate_angle(),
// an atan2 spanning (-180, 180]. So rotating counter-clockwise past zero
// stored a negative rotation, and three readers of rotation() test it
// against fixed positive sector ranges that a negative angle misses:
// get_diag_cursor() and is_edge_vertical() (selector.h), and
// PixmapItem::get_crop_edge_cursor() (moveitem.h). The resize cursor
// fell through to its default branch.
//
// FmlArchive::sanitize_item_transforms() folds the same way on load, so
// the bug healed on a save-and-reopen and came back as soon as the user
// rotated again - see tests/io/fml_transform_sanitize_test.cpp for that
// half.

namespace {

PixmapItem* make_picture()
{
    QImage img(100, 80, QImage::Format_RGB32);
    img.fill(Qt::gray);
    return new PixmapItem(img);
}

constexpr qreal kanchorX = 50;
constexpr qreal kanchorY = 40;

} // namespace

TEST(RotationNormalizationTest, ACounterClockwiseTurnIsStoredAsAPositiveAngle)
{
    std::unique_ptr<PixmapItem> item(make_picture());

    item->set_rotation(-90, QPointF(kanchorX, kanchorY));

    EXPECT_DOUBLE_EQ(item->rotation(), 270.0);
}

TEST(RotationNormalizationTest, AnAngleBeyondOneTurnIsFolded)
{
    std::unique_ptr<PixmapItem> item(make_picture());

    item->set_rotation(450, QPointF(kanchorX, kanchorY));

    EXPECT_DOUBLE_EQ(item->rotation(), 90.0);
}

// Both directions, and the exact multiples: fmod gives -0.0 for a whole
// negative turn, which compares equal to 0.0 but is worth pinning so a
// later reader that formats or serializes it does not start printing
// "-0".
TEST(RotationNormalizationTest, AWholeTurnIsZeroWhicheverWayItIsGiven)
{
    std::unique_ptr<PixmapItem> item(make_picture());

    item->set_rotation(360, QPointF(kanchorX, kanchorY));
    EXPECT_DOUBLE_EQ(item->rotation(), 0.0);

    item->set_rotation(-360, QPointF(kanchorX, kanchorY));
    EXPECT_DOUBLE_EQ(item->rotation(), 0.0);
}

// Every angle in [0, 360) has to pass through untouched, or the folding
// would be quietly changing rotations that were already fine.
TEST(RotationNormalizationTest, AnAngleAlreadyInRangeIsLeftAlone)
{
    std::unique_ptr<PixmapItem> item(make_picture());

    for (const qreal angle : {0.0, 0.5, 45.0, 180.0, 270.0, 359.5}) {
        item->set_rotation(angle, QPointF(kanchorX, kanchorY));
        EXPECT_DOUBLE_EQ(item->rotation(), angle);
    }
}

// The anchor compensation runs off the transform set_rotation() actually
// installed, so folding must not move the item. -90 and 270 are the same
// orientation and have to leave it in the same place.
TEST(RotationNormalizationTest, FoldingDoesNotMoveTheItem)
{
    const QPointF anchor(kanchorX, kanchorY);

    std::unique_ptr<PixmapItem> negative(make_picture());
    negative->setPos(QPointF(17, 23));
    const QPointF before = negative->mapToScene(anchor);
    negative->set_rotation(-90, anchor);
    const QPointF after = negative->mapToScene(anchor);

    EXPECT_NEAR(after.x(), before.x(), 1e-9);
    EXPECT_NEAR(after.y(), before.y(), 1e-9);

    std::unique_ptr<PixmapItem> positive(make_picture());
    positive->setPos(QPointF(17, 23));
    positive->set_rotation(270, anchor);

    EXPECT_NEAR(negative->pos().x(), positive->pos().x(), 1e-9);
    EXPECT_NEAR(negative->pos().y(), positive->pos().y(), 1e-9);
}

// The symptom the user sees. At 270 degrees the picture is sideways, so
// its top crop edge is dragged horizontally; given -90 - the same
// orientation - the old code matched no sector and answered as if the
// item were upright.
TEST(RotationNormalizationTest, TheCropCursorMatchesTheOrientationNotTheSign)
{
    const QPointF anchor(kanchorX, kanchorY);

    std::unique_ptr<PixmapItem> negative(make_picture());
    negative->set_rotation(-90, anchor);

    std::unique_ptr<PixmapItem> positive(make_picture());
    positive->set_rotation(270, anchor);

    EXPECT_EQ(negative->get_crop_edge_cursor(&PixmapItem::crop_edge_top),
              positive->get_crop_edge_cursor(&PixmapItem::crop_edge_top));
    EXPECT_EQ(negative->get_crop_edge_cursor(&PixmapItem::crop_edge_top),
              Qt::SizeHorCursor);

    std::unique_ptr<PixmapItem> upright(make_picture());
    EXPECT_EQ(upright->get_crop_edge_cursor(&PixmapItem::crop_edge_top),
              Qt::SizeVerCursor);
}

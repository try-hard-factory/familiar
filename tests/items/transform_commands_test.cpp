#include "commands.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QPointF>

// Undo for a scale/rotate drag used to recompute the previous state from
// the current one, which only works while the two never diverge. They do:
//
// - set_scale() refuses a non-positive value (selector.h), so dragging a
//   corner handle onto the opposite corner left the picture collapsed
//   while the command recorded factor 0 (undo then divided by it) or a
//   negative factor (undo was refused outright and did nothing).
// - Shift/Ctrl snapped the live rotation but mouseReleaseEvent() recorded
//   an UNSNAPPED delta, so undoing a 15-degree snapped turn whose raw
//   delta was 13.7 left 1.3 degrees behind.
//
// Both commands now store the absolute before/after instead.

namespace {

// 600x400, over kSmallImageSide, no alpha - an ordinary picture.
PixmapItem make_item()
{
    return PixmapItem(QImage(600, 400, QImage::Format_RGB32));
}

} // namespace

TEST(TransformCommandsTest, ScaleUndoRestoresTheCapturedScale)
{
    PixmapItem item = make_item();
    const QPointF anchor = item.center();

    // What mousePressEvent() captures when the drag starts.
    item.set_scale_orig_factor(1.0);
    // Where a drag past the anchor leaves it: set_scale() kept refusing
    // anything non-positive, so it stopped at the last value it accepted.
    item.set_scale(0.02, anchor);

    ScaleItemsByCommand cmd({&item}, anchor, /*ignoreFirstRedo=*/true);
    cmd.redo(); // the live drag already applied it - first redo is a no-op

    cmd.undo();
    EXPECT_DOUBLE_EQ(item.scale(), 1.0);

    cmd.redo();
    EXPECT_DOUBLE_EQ(item.scale(), 0.02);
}

// The factor such a drag implies is 0 or negative; undo must not compute
// with it at all. Nothing here passes a factor, which is the point - but
// the assertion still guards the outcome it used to produce: an infinite
// or refused scale.
TEST(TransformCommandsTest, ScaleUndoSurvivesACollapsedDrag)
{
    PixmapItem item = make_item();
    const QPointF anchor = item.center();

    item.set_scale_orig_factor(2.5);
    item.set_scale(0.001, anchor);

    ScaleItemsByCommand cmd({&item}, anchor, /*ignoreFirstRedo=*/true);
    cmd.redo();
    cmd.undo();

    EXPECT_DOUBLE_EQ(item.scale(), 2.5);
    EXPECT_GT(item.scale(), 0.0);
    EXPECT_TRUE(std::isfinite(item.scale()));
}

// The reported repro, with its exact numbers: a snapped 15-degree turn
// whose raw delta was 13.7 used to undo to 1.3 instead of 0.
TEST(TransformCommandsTest, RotateUndoRestoresZeroAfterASnappedTurn)
{
    PixmapItem item = make_item();
    const QPointF anchor = item.center();

    item.set_rotate_orig_degrees(0.0);
    item.set_rotation(15.0, anchor); // what Shift-drag snapped it to

    RotateItemsByCommand cmd({&item}, anchor, /*ignoreFirstRedo=*/true);
    cmd.redo();

    cmd.undo();
    EXPECT_DOUBLE_EQ(item.rotation(), 0.0);

    cmd.redo();
    EXPECT_DOUBLE_EQ(item.rotation(), 15.0);
}

// Repeated snapped turns are where the residue used to accumulate.
TEST(TransformCommandsTest, RotateUndoDoesNotAccumulateResidue)
{
    PixmapItem item = make_item();
    const QPointF anchor = item.center();

    for (int turn = 1; turn <= 5; ++turn) {
        item.set_rotate_orig_degrees(item.rotation());
        item.set_rotation(item.rotation() + 15.0, anchor);

        RotateItemsByCommand cmd({&item}, anchor, /*ignoreFirstRedo=*/true);
        cmd.redo();
        cmd.undo();

        EXPECT_DOUBLE_EQ(item.rotation(), 0.0) << "after turn " << turn;
    }
}

// A multi-selection drag replays one flat item list, and each entry has
// its own before/after - a single shared delta could not express that.
TEST(TransformCommandsTest, PerItemValuesAreRestoredIndependently)
{
    PixmapItem first = make_item();
    PixmapItem second = make_item();
    const QPointF anchor = first.center();

    first.set_scale_orig_factor(1.0);
    second.set_scale_orig_factor(3.0);
    first.set_scale(0.5, anchor);
    second.set_scale(6.0, anchor);

    ScaleItemsByCommand cmd({&first, &second}, anchor, /*ignoreFirstRedo=*/true);
    cmd.redo();
    cmd.undo();

    EXPECT_DOUBLE_EQ(first.scale(), 1.0);
    EXPECT_DOUBLE_EQ(second.scale(), 3.0);
}

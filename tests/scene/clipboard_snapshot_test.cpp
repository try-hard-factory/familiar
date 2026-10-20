#include "canvasscene.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QBuffer>
#include <QImage>
#include <QPointF>
#include <QUuid>

// The internal clipboard used to hold shared_ptrs to the LIVE items, so
// create_copy() ran at paste time: editing an original between Ctrl+C and
// Ctrl+V pasted the edited version (the reported repro rotated it 45
// degrees). It now holds snapshot_of() maps, which create_item_from_data()
// rebuilds - the same function the .fml loader and the import queue use.

namespace {

QImage test_image()
{
    QImage img(600, 400, QImage::Format_RGB32);
    img.fill(Qt::darkCyan);
    return img;
}

QGraphicsItem* as_graphics(IBaseItem* item)
{
    return dynamic_cast<QGraphicsItem*>(item);
}

} // namespace

// The repro, with its own numbers.
TEST(ClipboardSnapshotTest, EditingTheOriginalAfterCopyDoesNotAffectThePaste)
{
    PixmapItem original(test_image());
    const QPointF anchor = original.center();
    original.set_rotation(0.0, anchor);

    const QVariantMap snapshot = CanvasScene::snapshot_of(&original);

    // ...and now the user rotates what they copied.
    original.set_rotation(45.0, anchor);

    IBaseItem* pasted = CanvasScene::create_item_from_data(snapshot);
    ASSERT_NE(pasted, nullptr);
    EXPECT_DOUBLE_EQ(as_graphics(pasted)->rotation(), 0.0);
    EXPECT_DOUBLE_EQ(as_graphics(&original)->rotation(), 45.0);
    delete as_graphics(pasted);
}

TEST(ClipboardSnapshotTest, SnapshotCarriesEveryTransformField)
{
    PixmapItem original(test_image());
    const QPointF anchor = original.center();
    original.setPos(120.0, -75.0);
    original.setZValue(7.0);
    original.set_scale(2.5, anchor);
    original.set_rotation(30.0, anchor);

    const QVariantMap snapshot = CanvasScene::snapshot_of(&original);
    IBaseItem* rebuilt = CanvasScene::create_item_from_data(snapshot);
    ASSERT_NE(rebuilt, nullptr);

    auto* item = as_graphics(rebuilt);
    EXPECT_DOUBLE_EQ(item->scale(), 2.5);
    EXPECT_DOUBLE_EQ(item->rotation(), 30.0);
    EXPECT_DOUBLE_EQ(item->zValue(), 7.0);
    // pos() is set from the snapshot, then set_scale/set_rotation's own
    // anchor correction moves it - so compare what the snapshot stored.
    EXPECT_DOUBLE_EQ(snapshot.value("x").toReal(), original.pos().x());
    EXPECT_DOUBLE_EQ(snapshot.value("y").toReal(), original.pos().y());
    delete item;
}

// A picture that already has its encoded bytes must not be re-encoded on
// the way through the clipboard - same generational-loss problem the save
// path has (PixmapItem::encodedCache_).
TEST(ClipboardSnapshotTest, SnapshotCarriesTheEncodedBytes)
{
    const QImage source = test_image();
    QByteArray png;
    {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        ASSERT_TRUE(source.save(&buffer, "PNG"));
    }

    PixmapItem original(source);
    original.set_encoded_source(png, QStringLiteral("png"));

    const QVariantMap snapshot = CanvasScene::snapshot_of(&original);
    EXPECT_EQ(snapshot.value("encodedBytes").toByteArray(), png);

    auto* rebuilt = dynamic_cast<PixmapItem*>(
        CanvasScene::create_item_from_data(snapshot));
    ASSERT_NE(rebuilt, nullptr);
    const auto [bytes, format] = rebuilt->pixmap_to_bytes();
    EXPECT_EQ(bytes, png);
    EXPECT_EQ(format, QStringLiteral("png"));
    delete rebuilt;
}

TEST(ClipboardSnapshotTest, RebuiltItemsGetFreshIdentities)
{
    PixmapItem original(test_image());
    const QVariantMap snapshot = CanvasScene::snapshot_of(&original);
    EXPECT_EQ(snapshot.value("id").toUuid(), original.uid());

    const QList<IBaseItem*> copies = CanvasScene::build_from_snapshots(
        {snapshot});
    ASSERT_EQ(copies.size(), 1);
    EXPECT_NE(copies[0]->uid(), original.uid());
    delete as_graphics(copies[0]);
}

// A group and its member pasted together: the copy's child_ids must name
// the COPY of the member, not the original still sitting on the canvas.
TEST(ClipboardSnapshotTest, GroupMembershipIsRemappedToTheCopies)
{
    PixmapItem member(test_image());
    GroupItem group;
    group.set_child_ids({member.uid()});

    const QList<IBaseItem*> copies = CanvasScene::build_from_snapshots(
        {CanvasScene::snapshot_of(&group), CanvasScene::snapshot_of(&member)});
    ASSERT_EQ(copies.size(), 2);

    auto* groupCopy = dynamic_cast<GroupItem*>(copies[0]);
    ASSERT_NE(groupCopy, nullptr);
    ASSERT_EQ(groupCopy->child_ids().size(), 1);
    EXPECT_EQ(groupCopy->child_ids().first(), copies[1]->uid());
    EXPECT_NE(groupCopy->child_ids().first(), member.uid());

    for (IBaseItem* copy : copies) {
        delete as_graphics(copy);
    }
}

TEST(ClipboardSnapshotTest, AttachmentIsRemappedAndDroppedWhenAnchorIsAbsent)
{
    PixmapItem anchor(test_image());
    TextItem note;
    note.set_attached_to(anchor.uid());

    {
        const QList<IBaseItem*> both = CanvasScene::build_from_snapshots(
            {CanvasScene::snapshot_of(&anchor),
             CanvasScene::snapshot_of(&note)});
        ASSERT_EQ(both.size(), 2);
        EXPECT_EQ(both[1]->attached_to_uid(), both[0]->uid());
        for (IBaseItem* copy : both) {
            delete as_graphics(copy);
        }
    }

    // The note alone: its anchor is not part of the batch, so the copy
    // must land unattached rather than pinned to the original.
    const QList<IBaseItem*> alone = CanvasScene::build_from_snapshots(
        {CanvasScene::snapshot_of(&note)});
    ASSERT_EQ(alone.size(), 1);
    EXPECT_TRUE(alone[0]->attached_to_uid().isNull());
    delete as_graphics(alone[0]);
}

TEST(ClipboardSnapshotTest, TextContentSurvivesTheRoundTrip)
{
    TextItem note;
    note.setPlainText(QStringLiteral("remember this"));

    const QVariantMap snapshot = CanvasScene::snapshot_of(&note);
    auto* rebuilt = dynamic_cast<TextItem*>(
        CanvasScene::create_item_from_data(snapshot));
    ASSERT_NE(rebuilt, nullptr);
    EXPECT_EQ(rebuilt->toPlainText(), QStringLiteral("remember this"));
    delete rebuilt;
}

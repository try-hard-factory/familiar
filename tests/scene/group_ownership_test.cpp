#include "canvasscene.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QImage>
#include <QUndoStack>

// CanvasScene::find_owning_group() used to answer by scanning every item
// in the scene, with a dynamic_cast per item and a freshly built items()
// list per call. Three ancestor walks called it in a loop
// (GroupItem::fit_to_contain_children(), ::paint() and
// ::apply_lock_to_children()), and QGraphicsScene::changed() runs the
// first of those on every frame of a drag and every frame of a GIF -
// measured at 326ms per frame on 1000 pictures in 64 groups nested 5
// deep, against 0.6ms for the items() sweep that wraps it.
//
// It answers from an index now (member uid -> owning group), rebuilt
// lazily after a membership change. These tests are mostly about the
// part that can actually break: the index going stale.

namespace {

class GroupOwnershipTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
    }

    PixmapItem* add_picture(const QPointF& pos = QPointF(0, 0))
    {
        QImage img(32, 24, QImage::Format_RGB32);
        img.fill(Qt::gray);
        auto* item = new PixmapItem(img);
        item->setPos(pos);
        scene_->addItem(item);
        return item;
    }

    GroupItem* add_group(const QList<QUuid>& ids)
    {
        auto* group = new GroupItem();
        group->set_local_rect(QRectF(0, 0, 400, 300));
        scene_->addItem(group);
        group->set_child_ids(ids);
        return group;
    }

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
};

} // namespace

// ─── The index agrees with membership ──────────────────────────────────────

TEST_F(GroupOwnershipTest, FindsTheGroupThatListsTheItem)
{
    PixmapItem* member = add_picture();
    PixmapItem* outsider = add_picture(QPointF(500, 0));
    GroupItem* group = add_group({member->uid()});

    EXPECT_EQ(scene_->find_owning_group(member->uid()), group);
    EXPECT_EQ(scene_->find_owning_group(outsider->uid()), nullptr);
}

TEST_F(GroupOwnershipTest, AddingAMemberIsVisibleAtOnce)
{
    PixmapItem* member = add_picture();
    GroupItem* group = add_group({});
    // Query first, so the index is already built and cached - the point
    // is that the membership change drops it, not that it was never
    // built.
    ASSERT_EQ(scene_->find_owning_group(member->uid()), nullptr);

    group->add_child_id(member->uid());

    EXPECT_EQ(scene_->find_owning_group(member->uid()), group);
}

TEST_F(GroupOwnershipTest, RemovingAMemberIsVisibleAtOnce)
{
    PixmapItem* member = add_picture();
    GroupItem* group = add_group({member->uid()});
    ASSERT_EQ(scene_->find_owning_group(member->uid()), group);

    group->remove_child_id(member->uid());

    EXPECT_EQ(scene_->find_owning_group(member->uid()), nullptr);
}

TEST_F(GroupOwnershipTest, ReplacingTheWholeMembershipListIsVisibleAtOnce)
{
    PixmapItem* first = add_picture();
    PixmapItem* second = add_picture(QPointF(100, 0));
    GroupItem* group = add_group({first->uid()});
    ASSERT_EQ(scene_->find_owning_group(first->uid()), group);

    group->set_child_ids({second->uid()});

    EXPECT_EQ(scene_->find_owning_group(first->uid()), nullptr);
    EXPECT_EQ(scene_->find_owning_group(second->uid()), group);
}

TEST_F(GroupOwnershipTest, AGroupLeavingTheSceneStopsOwningAnything)
{
    PixmapItem* member = add_picture();
    GroupItem* group = add_group({member->uid()});
    ASSERT_EQ(scene_->find_owning_group(member->uid()), group);

    // removeItem() releases the scene's shared_ptr (attachedItems_),
    // which deletes the group - `group` dangles from here on, so it is
    // only ever compared against, never dereferenced.
    scene_->removeItem(group);

    EXPECT_EQ(scene_->find_owning_group(member->uid()), nullptr);
}

TEST_F(GroupOwnershipTest, ClearingTheSceneEmptiesTheIndex)
{
    PixmapItem* member = add_picture();
    const QUuid memberUid = member->uid();
    add_group({memberUid});
    ASSERT_NE(scene_->find_owning_group(memberUid), nullptr);

    scene_->clear();

    EXPECT_EQ(scene_->find_owning_group(memberUid), nullptr);
}

// ─── owning_group_chain() ──────────────────────────────────────────────────

TEST_F(GroupOwnershipTest, TheChainIsEmptyForAnUngroupedItem)
{
    PixmapItem* loner = add_picture();
    EXPECT_TRUE(scene_->owning_group_chain(loner->uid()).isEmpty());
}

TEST_F(GroupOwnershipTest, TheChainWalksEveryLevelInnermostFirst)
{
    PixmapItem* leaf = add_picture();
    GroupItem* inner = add_group({leaf->uid()});
    GroupItem* middle = add_group({inner->uid()});
    GroupItem* outer = add_group({middle->uid()});

    EXPECT_EQ(scene_->owning_group_chain(leaf->uid()),
              QList<GroupItem*>({inner, middle, outer}));
    EXPECT_EQ(scene_->owning_group_chain(middle->uid()),
              QList<GroupItem*>({outer}));
    EXPECT_TRUE(scene_->owning_group_chain(outer->uid()).isEmpty());
}

TEST_F(GroupOwnershipTest, TheChainStopsOnAnOwnershipCycle)
{
    // Nothing in GroupItem prevents this - add_child_id() only logs a
    // warning when a uid gains a second owner. The three ancestor walks
    // this chain replaced were plain `for (a = this; a; a = owner(a))`
    // loops with no guard at all, so a cycle spun forever. If the guard
    // is ever removed this test hangs rather than fails.
    GroupItem* first = add_group({});
    GroupItem* second = add_group({});
    first->add_child_id(second->uid());
    second->add_child_id(first->uid());

    const QList<GroupItem*> fromFirst = scene_->owning_group_chain(
        first->uid());

    // Terminated, and visited each group at most once.
    EXPECT_EQ(fromFirst, QList<GroupItem*>({second}));
    EXPECT_EQ(scene_->owning_group_chain(second->uid()),
              QList<GroupItem*>({first}));
}

TEST_F(GroupOwnershipTest, AGroupListedAmongItsOwnMembersIsACycleToo)
{
    GroupItem* group = add_group({});
    group->add_child_id(group->uid());

    EXPECT_TRUE(scene_->owning_group_chain(group->uid()).isEmpty());
}

// ─── Cost ──────────────────────────────────────────────────────────────────

TEST_F(GroupOwnershipTest, OnChangeDoesNotGrowQuadraticallyWithTheItemCount)
{
    // A wall-clock test, so the bound is on the SHAPE of the growth, not
    // on any absolute number of milliseconds: four times the items must
    // not cost dramatically more than four times the time. The linear
    // scan this replaced measured ~5.4x per 2.5x items (roughly
    // quadratic, and worse once groups nest), so the ratio below sits
    // far above anything linear and far below what the old code did.
    constexpr int ksmallPictures = 150;
    constexpr int klargePictures = 600;
    constexpr int kgroups = 20;
    constexpr int kframes = 30;

    const auto measure_ns = [this](int pictures) {
        scene_->clear();
        QList<QUuid> ids;
        for (int i = 0; i < pictures; ++i) {
            ids.append(add_picture(QPointF((i % 20) * 50, (i / 20) * 40))
                           ->uid());
        }
        // Nested two deep, so the ancestor walk actually has somewhere
        // to walk - that is where the old cost exploded.
        QList<GroupItem*> inner;
        const int perGroup = pictures / kgroups;
        for (int g = 0; g < kgroups; ++g) {
            QList<QUuid> members;
            for (int k = 0; k < perGroup; ++k) {
                members.append(ids[(g * perGroup + k) % ids.size()]);
            }
            inner.append(add_group(members));
        }
        for (int i = 0; i + 1 < inner.size(); i += 2) {
            add_group({inner[i]->uid(), inner[i + 1]->uid()});
        }

        // One warm-up frame: the first one does the real refit, every
        // later one takes fit_to_contain_children()'s "already fits"
        // early-out, which is the per-frame path being measured.
        scene_->on_change();
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < kframes; ++i) {
            scene_->on_change();
        }
        return timer.nsecsElapsed() / kframes;
    };

    const qint64 smallNs = measure_ns(ksmallPictures);
    const qint64 largeNs = measure_ns(klargePictures);

    if (smallNs < 1000) {
        GTEST_SKIP() << "baseline too fast to compare meaningfully ("
                     << smallNs << "ns)";
    }
    const double itemRatio = double(klargePictures) / ksmallPictures;
    const double timeRatio = double(largeNs) / double(smallNs);
    EXPECT_LT(timeRatio, itemRatio * 2.0)
        << "per-frame on_change() went from " << smallNs << "ns to " << largeNs
        << "ns for " << itemRatio << "x the items (" << timeRatio << "x)";
}

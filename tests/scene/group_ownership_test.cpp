#include "canvasscene.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

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

TEST_F(GroupOwnershipTest, OnChangeDoesNotGrowWithTheNumberOfGroups)
{
    // The cost this replaced was O(groups x items): every group's
    // fit_to_contain_children() asked find_owning_group(), and that
    // answer came from a fresh items() sweep with a dynamic_cast per
    // item. It answers from an index now, so the per-frame cost should
    // stop caring how many groups there are.
    //
    // Groups, deliberately, and not pictures: the per-frame cost is
    // dominated by on_change()'s own items() sweep either way, and
    // items() sorts by stacking order, so growing the picture count
    // grows BOTH versions superlinearly and separates them by almost
    // nothing. Holding the pictures fixed and varying the groups
    // isolates the term that actually changed.
    constexpr int kpictures = 600;
    constexpr int kfewGroups = 10;
    constexpr int kmanyGroups = 40;
    constexpr int kframes = 30;
    // The timed loop runs this many times and the FASTEST is kept.
    // Scheduling noise only ever adds time, so a single measurement has
    // a long tail upwards and none downwards; a minimum is the stable
    // estimator of a one-sided distribution, a mean is not.
    constexpr int kruns = 5;

    const auto measure_ns = [this](int groups) {
        scene_->clear();
        QList<QUuid> ids;
        for (int i = 0; i < kpictures; ++i) {
            ids.append(add_picture(QPointF((i % 20) * 50, (i / 20) * 40))
                           ->uid());
        }
        // Nested two deep, so the ancestor walk actually has somewhere
        // to walk - that is where the old cost exploded.
        QList<GroupItem*> inner;
        const int perGroup = kpictures / groups;
        for (int g = 0; g < groups; ++g) {
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

        qint64 bestNs = std::numeric_limits<qint64>::max();
        for (int run = 0; run < kruns; ++run) {
            QElapsedTimer timer;
            timer.start();
            for (int i = 0; i < kframes; ++i) {
                scene_->on_change();
            }
            bestNs = std::min(bestNs, timer.nsecsElapsed() / kframes);
        }
        return bestNs;
    };

    const qint64 fewNs = measure_ns(kfewGroups);
    const qint64 manyNs = measure_ns(kmanyGroups);

    if (fewNs < 1000) {
        GTEST_SKIP() << "baseline too fast to compare meaningfully ("
                     << fewNs << "ns)";
    }
    const double groupRatio = double(kmanyGroups) / kfewGroups;
    const double timeRatio = double(manyNs) / double(fewNs);
    // Four times the groups over the same pictures. The old scan paid
    // that in full; the index should be close to flat, so the bound
    // sits halfway between the two rather than just under the old one.
    EXPECT_LT(timeRatio, groupRatio / 2.0)
        << "per-frame on_change() went from " << fewNs << "ns to " << manyNs
        << "ns for " << groupRatio << "x the groups (" << timeRatio << "x)";
}

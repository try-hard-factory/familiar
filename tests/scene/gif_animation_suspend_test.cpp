#include "canvasscene.h"
#include "canvasview.h"
#include "commands.h"
#include "mainwindow.h"
#include "moveitem.h"
#include "tabpane.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QUndoStack>

#include <memory>

// QMovie's clock runs off the event loop and knows nothing about whether
// its frames reach a screen, so every animated picture in the app kept
// decoding a frame and repainting an item forever - in a tab that was
// not the open one, and in a GIF the user had deleted, which lives on in
// the undo stack waiting to be undone.
//
// Measured on a 150-frame 1920x1080 GIF (12.3 MB file): a QMovie nothing
// displays at all produced 76 frameChanged ticks and +586 MB of RSS in
// three seconds, and its CacheAll frame cache plateaus at 1195 MB. Hence
// two different answers: a hidden tab's animations are paused (they are
// coming back), while one that has left the scene drops its QMovie
// entirely (the cache is the expensive part).

namespace {

// A real two-frame animated GIF, 4x4, black then white. Same literal as
// tests/io/scene_export_test.cpp and items/gif_frame_count_test.cpp.
QByteArray gif_bytes()
{
    return QByteArray::fromBase64("R0lGODlhBAAEAPAAAAAAAP///yH/C05FVFNDQVBFMi4wAwEAAAAh+QQA"
                                  "CgAAACwAAAAABAAEAAACBwQAAAAAACgAIfkEAAoAAAAsAAAAAAQABAAA"
                                  "AgdMkiRJkiQpADs=");
}

class GifAnimationSuspendTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
    }

    GifItem* add_gif(CanvasScene* scene)
    {
        auto* item = new GifItem(gif_bytes(), QStringLiteral("anim.gif"));
        scene->addItem(item);
        return item;
    }

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
};

} // namespace

// ─── Leaving and re-entering a scene ───────────────────────────────────────

TEST_F(GifAnimationSuspendTest, TheMovieGoesAwayWhenTheItemLeavesTheScene)
{
    GifItem* item = add_gif(scene_.get());
    ASSERT_NE(item->movie(), nullptr);
    // CanvasScene::addItem() hands its shared reference back on
    // removeItem(), so without one of our own the item would be
    // destroyed by the very call under test.
    const std::shared_ptr<IBaseItem> ref = item->acquire_shared();

    scene_->removeItem(item);

    EXPECT_EQ(item->movie(), nullptr);
}

TEST_F(GifAnimationSuspendTest, TheMovieComesBackWhenTheItemReturns)
{
    GifItem* item = add_gif(scene_.get());
    const std::shared_ptr<IBaseItem> ref = item->acquire_shared();
    scene_->removeItem(item);
    ASSERT_EQ(item->movie(), nullptr);

    scene_->addItem(item);

    ASSERT_NE(item->movie(), nullptr);
    EXPECT_TRUE(item->is_playing());
}

// The path a user actually takes: Delete, then Ctrl+Z.
TEST_F(GifAnimationSuspendTest, AnUndoneDeleteBringsTheAnimationBack)
{
    GifItem* item = add_gif(scene_.get());
    ASSERT_NE(item->movie(), nullptr);

    undoStack_.push(
        new DeleteItemsCommand(scene_.get(), QList<QGraphicsItem*>{item}));
    EXPECT_EQ(item->movie(), nullptr);

    undoStack_.undo();

    ASSERT_NE(item->movie(), nullptr);
    EXPECT_TRUE(item->is_playing());
}

TEST_F(GifAnimationSuspendTest, AGifPausedByTheUserComesBackPaused)
{
    GifItem* item = add_gif(scene_.get());
    item->pause();
    ASSERT_FALSE(item->is_playing());
    const std::shared_ptr<IBaseItem> ref = item->acquire_shared();

    scene_->removeItem(item);
    scene_->addItem(item);

    EXPECT_FALSE(item->is_playing());
    // And it can still be started afterwards - a rebuilt movie that came
    // back paused is NotRunning, which setPaused(false) alone ignores.
    item->play();
    EXPECT_TRUE(item->is_playing());
}

TEST_F(GifAnimationSuspendTest, ThePlaybackSpeedSurvivesTheRoundTrip)
{
    GifItem* item = add_gif(scene_.get());
    item->set_speed_percent(25);
    const std::shared_ptr<IBaseItem> ref = item->acquire_shared();

    scene_->removeItem(item);
    scene_->addItem(item);

    EXPECT_EQ(item->speed_percent(), 25);
}

// ─── Hidden tabs ───────────────────────────────────────────────────────────

TEST_F(GifAnimationSuspendTest, AGifInAHiddenTabStopsAnimating)
{
    CanvasView* first = window_.tab_pane().current_widget();
    ASSERT_NE(first, nullptr);
    GifItem* item = add_gif(first->scene());
    ASSERT_TRUE(item->is_playing());

    // Opens a second tab and switches to it, hiding the first.
    window_.tab_pane().add_new_untitled_tab();
    EXPECT_FALSE(item->is_playing());
    // Its movie is kept, unlike an item that left the scene - the tab is
    // coming back.
    EXPECT_NE(item->movie(), nullptr);

    window_.tab_pane().set_current_index(0);

    EXPECT_TRUE(item->is_playing());
}

TEST_F(GifAnimationSuspendTest, ComingBackToATabDoesNotRestartAPausedGif)
{
    CanvasView* first = window_.tab_pane().current_widget();
    ASSERT_NE(first, nullptr);
    GifItem* item = add_gif(first->scene());
    item->pause();

    window_.tab_pane().add_new_untitled_tab();
    window_.tab_pane().set_current_index(0);

    EXPECT_FALSE(item->is_playing());
}

// A load finishing into a tab that is not the open one adds its items
// through add_queued_items(), and a GifItem animates from the moment it
// is constructed - so the suspension has to reach an animation that did
// not exist when the tab was hidden.
TEST_F(GifAnimationSuspendTest, AGifArrivingInAHiddenSceneDoesNotStartUp)
{
    scene_->suspend_animations();

    GifItem* item = add_gif(scene_.get());

    EXPECT_FALSE(item->is_playing());

    scene_->resume_animations();

    EXPECT_TRUE(item->is_playing());
}

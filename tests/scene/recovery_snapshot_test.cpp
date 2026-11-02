#include "canvasview.h"
#include "commands.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QUndoStack>

// MainWindow's recovery timer snapshotted every tab whose is_modified()
// was true, and that stays true until the next MANUAL save - so a dirty
// tab nobody was touching got rewritten in full every 30 seconds
// forever. FmlArchive::save() builds the whole archive in heap memory
// and writes it through QSaveFile::commit() (an fdatasync),
// synchronously on the GUI thread: measured at 2.1s per snapshot on a
// 300-picture board with nothing at all having changed.
//
// These pin the gate that decides whether a snapshot is due. See
// CanvasView::content_revision() for why it is a counter of its own
// rather than QUndoStack::index().

namespace {

class RecoverySnapshotTest : public ::testing::Test
{
protected:
    // Through TabPane rather than constructing a bare CanvasView: a tab
    // only gets its ProjectSettings here (tabpane.cpp), and that is what
    // actually stores the modified flag - is_modified() dereferences a
    // null pointer without it.
    void SetUp() override
    {
        window_.tab_pane().add_new_untitled_tab();
        view_ = window_.tab_pane().current_widget();
        ASSERT_NE(view_, nullptr);
        ASSERT_FALSE(view_->is_modified());
    }

    // A command of the kind ordinary editing pushes, so the undo stack
    // moves the way it really does.
    void push_an_edit()
    {
        auto* item = new PixmapItem(QImage(32, 24, QImage::Format_RGB32));
        view_->undo_stack()->push(
            new InsertItemsCommand(view_->scene(), {item}));
    }

    MainWindow window_;
    CanvasView* view_ = nullptr;
};

} // namespace

TEST_F(RecoverySnapshotTest, AnUntouchedTabIsNeverSnapshotted)
{
    ASSERT_FALSE(view_->is_modified());
    EXPECT_FALSE(view_->needs_recovery_snapshot());
}

TEST_F(RecoverySnapshotTest, AnEditedTabIsDueASnapshot)
{
    push_an_edit();

    ASSERT_TRUE(view_->is_modified());
    EXPECT_TRUE(view_->needs_recovery_snapshot());
}

// The whole point of the change.
TEST_F(RecoverySnapshotTest, AnIdleDirtyTabIsNotSnapshottedAgainAndAgain)
{
    push_an_edit();
    ASSERT_TRUE(view_->needs_recovery_snapshot());

    view_->mark_recovery_snapshot_taken();

    // Still modified - nothing was saved - but the snapshot on disk
    // already holds exactly this, so every later tick must skip it.
    EXPECT_TRUE(view_->is_modified());
    EXPECT_FALSE(view_->needs_recovery_snapshot());
    EXPECT_FALSE(view_->needs_recovery_snapshot());
}

TEST_F(RecoverySnapshotTest, AFurtherEditMakesItDueAgain)
{
    push_an_edit();
    view_->mark_recovery_snapshot_taken();
    ASSERT_FALSE(view_->needs_recovery_snapshot());

    push_an_edit();

    EXPECT_TRUE(view_->needs_recovery_snapshot());
}

TEST_F(RecoverySnapshotTest, UndoingIsItselfAChangeWorthSnapshotting)
{
    push_an_edit();
    push_an_edit();
    view_->mark_recovery_snapshot_taken();
    ASSERT_FALSE(view_->needs_recovery_snapshot());

    view_->undo_stack()->undo();

    EXPECT_TRUE(view_->needs_recovery_snapshot());
}

// The case that rules out comparing QUndoStack::index() directly: a new
// command after an undo truncates the stack, so the index comes back to
// a value already snapshotted while the content is completely
// different. Comparing indices would skip this and leave the first edit
// on disk as the thing a crash recovers.
TEST_F(RecoverySnapshotTest, AnEditAfterAnUndoIsDueEvenAtTheSameUndoIndex)
{
    push_an_edit();
    const int indexAtSnapshot = view_->undo_stack()->index();
    view_->mark_recovery_snapshot_taken();
    ASSERT_FALSE(view_->needs_recovery_snapshot());

    view_->undo_stack()->undo();
    push_an_edit();

    ASSERT_EQ(view_->undo_stack()->index(), indexAtSnapshot)
        << "the test no longer reproduces the index collision it is about";
    EXPECT_TRUE(view_->needs_recovery_snapshot());
}

// FileActions marks a repaired-on-load project modified without
// anything ever reaching the undo stack - the index stays 0. That
// project is exactly the one most in need of a snapshot.
TEST_F(RecoverySnapshotTest, ModifiedWithoutAnyUndoActivityIsStillDue)
{
    view_->set_modified(true);

    ASSERT_EQ(view_->undo_stack()->index(), 0);
    EXPECT_TRUE(view_->needs_recovery_snapshot());

    view_->mark_recovery_snapshot_taken();
    EXPECT_FALSE(view_->needs_recovery_snapshot());
}

// Clearing the flag is what a successful save does; it must not look
// like a reason to write another snapshot.
TEST_F(RecoverySnapshotTest, SavingDoesNotMakeATabDueASnapshot)
{
    push_an_edit();
    view_->mark_recovery_snapshot_taken();

    view_->undo_stack()->setClean();
    view_->set_modified(false);

    EXPECT_FALSE(view_->is_modified());
    EXPECT_FALSE(view_->needs_recovery_snapshot());
}

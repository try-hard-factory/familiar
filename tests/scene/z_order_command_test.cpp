#include "commands.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QUndoStack>

// Raise/lower used to change zValue straight from CanvasScene, pushing
// nothing: Ctrl+Z did not undo it, and - because "modified" is derived
// from the undo stack's clean state alone (CanvasView::
// on_undo_clean_changed()) - the tab still looked saved, so closing it
// dropped the new layer order without asking. It is a command now.

namespace {

// Returned as a prvalue, so C++17 elision constructs it straight into the
// caller's variable - a QGraphicsItem is neither copyable nor movable, so
// naming a local inside here first would not compile.
PixmapItem make_item()
{
    return PixmapItem(QImage(40, 30, QImage::Format_RGB32));
}

} // namespace

TEST(ZOrderCommandTest, UndoRestoresEveryItemsOwnZValue)
{
    PixmapItem first = make_item();
    PixmapItem second = make_item();
    first.setZValue(1.0);
    second.setZValue(5.0);

    ChangeZOrderCommand cmd({&first, &second},
                            {11.0, 15.0},
                            QStringLiteral("Raise to top"));
    cmd.redo();
    EXPECT_DOUBLE_EQ(first.zValue(), 11.0);
    EXPECT_DOUBLE_EQ(second.zValue(), 15.0);

    cmd.undo();
    EXPECT_DOUBLE_EQ(first.zValue(), 1.0);
    EXPECT_DOUBLE_EQ(second.zValue(), 5.0);

    cmd.redo();
    EXPECT_DOUBLE_EQ(first.zValue(), 11.0);
    EXPECT_DOUBLE_EQ(second.zValue(), 15.0);
}

// What the repro measured: the stack stayed at 0, so nothing was
// undoable and nothing marked the document modified.
TEST(ZOrderCommandTest, PushingItMakesTheStackDirty)
{
    PixmapItem item = make_item();
    item.setZValue(2.0);
    QUndoStack stack;
    stack.setClean();
    ASSERT_TRUE(stack.isClean());
    ASSERT_EQ(stack.count(), 0);

    stack.push(new ChangeZOrderCommand({&item},
                                       {9.0},
                                       QStringLiteral("Raise to top")));

    EXPECT_EQ(stack.count(), 1);
    EXPECT_FALSE(stack.isClean()); // this is what drives set_modified(true)
    EXPECT_DOUBLE_EQ(item.zValue(), 9.0);

    stack.undo();
    EXPECT_TRUE(stack.isClean());
    EXPECT_DOUBLE_EQ(item.zValue(), 2.0);
}

TEST(ZOrderCommandTest, UndoHistoryTellsTheTwoDirectionsApart)
{
    PixmapItem item = make_item();
    item.setZValue(0.0);
    QUndoStack stack;
    stack.push(new ChangeZOrderCommand({&item},
                                       {3.0},
                                       QStringLiteral("Raise to top")));
    stack.push(new ChangeZOrderCommand({&item},
                                       {-3.0},
                                       QStringLiteral("Lower to bottom")));

    EXPECT_EQ(stack.text(0), QStringLiteral("Raise to top"));
    EXPECT_EQ(stack.text(1), QStringLiteral("Lower to bottom"));
}

// An empty selection must not reach *min_element()/*max_element() at all.
TEST(ZOrderCommandTest, EmptySelectionIsANoOp)
{
    QUndoStack stack;
    stack.push(new ChangeZOrderCommand({}, {}, QStringLiteral("Raise to top")));
    EXPECT_EQ(stack.count(), 1);
    stack.undo();
    SUCCEED(); // no crash, nothing to assert about
}

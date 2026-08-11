#pragma once

#include <QDockWidget>
#include <QMetaObject>
#include <QSet>
#include <QUuid>

class GifItem;

class CanvasScene;
class CanvasView;
class QGraphicsItem;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

// Scene outliner (Max: "фича с иерархией... слева открывается панель с
// иерархией"), toggled via the "hierarchy" action (Ctrl+J). One
// long-lived instance owned by MainWindow, rebound to whichever tab is
// currently active (see MainWindow::resyncActionsForTab()) rather than
// one instance per CanvasView - it's chrome, not per-document state.
//
// The tree mixes TWO different relationships from the underlying flat
// scene into one hierarchy: group membership (GroupItem::child_ids())
// AND attachment (IBaseItem::attachedToUid(), roadmap step 25 - a
// TextItem note attached to a picture, or, current, a PixmapItem
// attached to another one, Max: "аттачить картинку к картинке") - an
// attached item nests under its anchor wherever that anchor itself ends
// up (top-level or inside a group), not as a flat sibling of the
// anchor's other group-mates.
//
// Interactive (current, Max: "в пьюрефе эта панель интерактивная...
// можно перемещать элементы... приаттачивать текст к картинке, или
// добавлять в группу... деаттачить") - dragging a row onto a Group node
// adds/transfers it there (CanvasScene::add_to_group()), onto a
// picture node attaches it (CanvasScene::attach_item_to()), onto empty
// space detaches AND/OR leaves its group, landing it top-level
// (CanvasScene::detach_item()) - see handleTreeDrop_()'s own comment
// for the full dispatch. The tree itself never reparents nodes
// directly for any of this; a drop only ever translates into a
// CanvasScene call, whose own undo command triggers the next rebuild
// below - same "tree is a read-only projection of the scene" principle
// as everything else in this class.
//
// Rebuilt wholesale via scheduleRefresh(),
// hooked to QUndoStack::indexChanged in MainWindow::resyncActionsForTab()
// - real structural mutations (group/ungroup/attach/delete/move/...) all
// go through a QUndoCommand, so that signal is a clean, low-frequency
// trigger. The one thing that DOESN'T go through the undo stack is a
// background file load (FileActions::loadFmlIntoCurrentTab()'s
// add_queued_items() call) - that call site explicitly calls
// MainWindow::notifyStructuralChange() itself instead (Max: "если
// чекбокс был поставлен на иерархии, то при повторно запуске эта панель
// пустая"). An earlier version hooked CanvasScene::changed() instead,
// reasoning it covers literally everything - it does, but it ALSO fires
// continuously (every few ms) for as long as any GifItem is animating,
// not just during a load, which either starved a debounce forever or
// (once throttled) forced a pointless rebuild every ~150ms just from a
// gif playing (Max: "может не ребилдить на обновлении гифки"). Dropped
// in favor of the indexChanged + explicit-notify combo above, which
// doesn't fire for animation at all.
class HierarchyPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit HierarchyPanel(QWidget* parent = nullptr);

    // Called from MainWindow::resyncActionsForTab() - both null when no
    // tab is active (transient zero-tab moment during tab-close).
    void setScene(CanvasScene* scene, CanvasView* view);

    // Rebuilds the tree from the currently bound scene right away.
    // Cheap to call liberally - a no-op while the panel itself is
    // hidden (rebuilt lazily on the next showEvent instead, see
    // dirty_). Called from setScene() (always) and scheduleRefresh()'s
    // throttle timer.
    void refresh();

    // Throttled version - arms a short single-shot timer instead of
    // rebuilding synchronously, in case its trigger (QUndoStack::
    // indexChanged) ever fires several times back to back (e.g. a macro
    // command). Intentionally a THROTTLE (only re-arms once the pending
    // timer has fired), not a debounce (restart-on-every-call) - a
    // debounce never gets a quiet gap to elapse if its source ever fires
    // continuously (this bit a previous, since-abandoned design that
    // hooked the much noisier CanvasScene::changed() - see this class's
    // own comment above). At most one rebuild lands every ~150ms while
    // calls keep coming in.
    void scheduleRefresh();

    // Highlights whichever row(s) match the scene's current selection,
    // without a full rebuild - call after CanvasScene::selectionChanged.
    void syncSelectionFromScene();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuild_();
    void addItemNode_(QTreeWidgetItem* parent, QGraphicsItem* item, QSet<QUuid>& added);
    QTreeWidgetItem* makeNode_(QGraphicsItem* item);
    // Live-updates node's icon on every GIF frame instead of waiting for
    // the next full rebuild_() - see its own comment for why this is
    // safe now (it wasn't, back when a rebuild trigger was tied to the
    // same signal a playing gif spams).
    void connectGifAnimation_(QTreeWidgetItem* node, GifItem* gif);
    void onItemClicked_(QTreeWidgetItem* node);
    void onItemDoubleClicked_(QTreeWidgetItem* node);
    // Interactive drag-and-drop (current, Max: "в пьюрефе эта панель
    // интерактивная... можно перемещать элементы... приаттачивать
    // текст к картинке, или добавлять в группу... деаттачить"). target
    // is null when dropped on empty space below the last root row
    // (= "make it top-level" - detach and/or leave its group). Called
    // from the tree's own dropEvent() (see the HierarchyTreeWidget
    // subclass local to hierarchy_panel.cpp) - dispatches to a
    // CanvasScene call based on target's type; the tree itself never
    // reparents nodes directly, it's always a read-only projection
    // rebuilt from the scene (see this class's own header comment).
    void handleTreeDrop_(QTreeWidgetItem* dragged, QTreeWidgetItem* target);

    QTreeWidget* tree_ = nullptr;
    CanvasScene* scene_ = nullptr;
    CanvasView* view_ = nullptr;
    QTimer* rebuildTimer_ = nullptr;
    bool dirty_ = true;
    // Guards against the click handler's own scene->setSelected() call
    // immediately bouncing back through CanvasScene::selectionChanged
    // -> syncSelectionFromScene() and re-touching the tree mid-click.
    bool syncingSelection_ = false;
    // One entry per currently-animating gif node, torn down at the top
    // of every rebuild_() BEFORE tree_->clear() destroys the
    // QTreeWidgetItems these lambdas close over - see rebuild_().
    QList<QMetaObject::Connection> gifIconConnections_;
};

#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QMutex>
#include <QRubberBand>
#include <QSet>
#include <QUuid>
#include <QVariantMap>

#include <memory>
#include <string>
#include <queue>

class MainWindow;
class project_settings;
class TextItem;
class PixmapItem;
class RubberbandItem;
class MultiSelectItem;
class GroupItem;
class QUndoStack;
class IBaseItem;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT
public:
    enum ESceneMode {
        kNone = 0,
        kMoveMode = 1,
        kRubberbandMode = 2,
    };

signals:
    void cursor_changed(QCursor);
    void cursor_cleared();
    // Fired by TextItem::enter_edit_mode()/exit_edit_mode() (nullptr on
    // exit) - CanvasView shows/hides the floating text toolbar on this.
    void edit_item_changed(TextItem* item);

public:
    // TextItem is not in a position to emit our signals itself (signals
    // are protected in Qt) - it calls this instead.
    void notify_edit_item_changed(TextItem* item)
    {
        emit edit_item_changed(item);
    }

public:
    // Holds data for an item queued via add_item_later(), consumed by
    // add_queued_items().
    struct QueuedItemData
    {
        QVariantMap data;
        bool selected = false;
    };

    CanvasScene(MainWindow& mw,
                uint64_t& zc,
                QUndoStack* undoStack,
                QGraphicsScene* scene = 0);
    ~CanvasScene();

    void addItem(QGraphicsItem* item);
    void removeItem(QGraphicsItem* item);
    // Detaches every item still in the scene via our own removeItem()
    // above, instead of leaving it to QGraphicsScene::clear()/its own
    // destructor - both of those delete remaining items directly,
    // bypassing our removeItem() override. If attachedItems_ still held
    // a shared_ptr for one of them at that point, we'd double-free: Qt
    // deletes the object directly, then attachedItems_'s own shared_ptr
    // destruction tries to delete it again.
    void detachAllItems();
    void cancel_active_modes();
    void end_rubberband_mode();
    void cancel_crop_mode();
    void copy_selection_to_internal_clipboard();
    void paste_from_internal_clipboard(QPointF position);
    void raise_to_top();
    void lower_to_bottom();
    // Raises the CURRENT selection (however it was built - single
    // click, ctrl+click accumulation, rubber-band sweep) to a fresh
    // consecutive z band above everything else in the scene. A selected
    // GroupItem is expanded into its own cluster (group + members, none
    // of which are individually Qt-selected - see GroupItem's class
    // comment, moveitem.h) before raising, recursively, via pre-order
    // DFS - direct children sorted among themselves by their own current
    // z, same as the top-level selection roots - so each nested
    // subgroup's whole cluster stays one contiguous block instead of
    // interleaving with a sibling subgroup's (a flat sort-by-z of
    // everything at once doesn't guarantee that if stale z values from
    // an earlier interaction left a subgroup's members not already
    // contiguous - confirmed with Max via a debug capture). Called from
    // on_selection_change() - pre-existing per-item bring-to-front
    // (ItemMixin::on_selected_change()) only ever raised the FIRST item
    // of a new selection (guarded on "nothing was already selected"), so
    // a second/third ctrl-clicked or rubber-banded item stayed at its
    // old z, potentially still buried under unrelated content.
    void raise_selection_to_front();
    // Drag-and-drop-to-add - called from
    // mouseReleaseEvent() after committing a body drag, with the
    // cursor's OWN scene position (not any dragged item's center/bounds
    // - matches normal drag-and-drop expectations: wherever you're
    // actually pointing at release is the target).
    // Every LOOSE item among movedItems (not already a member of any
    // group) becomes a new member of whichever unlocked,
    // drag_drop_enabled() group's area contains the cursor - topmost by
    // z if more than one candidate overlaps there (e.g. a subgroup
    // nested inside an outer group that also qualifies). A GroupItem
    // itself is a valid candidate too - dragging a whole subgroup onto
    // another group nests it exactly like the
    // "select both + Ctrl+G" flow would, just via drag instead. Never a
    // candidate for itself or one of its OWN descendants though (see
    // forbidden_drop_targets()) - that would be a membership cycle.
    void maybe_add_dropped_items_to_group(
        const QList<QGraphicsItem*>& movedItems, const QPointF& cursorScenePos);
    // Hierarchy panel drag-and-drop (current) - explicit-target
    // counterpart of maybe_add_dropped_items_to_group() above: that one
    // picks its target group by CURSOR POSITION (a canvas drag has no
    // other way to say "here"); the tree already knows exactly which
    // GroupItem node was dropped on, no spatial lookup needed. Same
    // add-or-transfer logic and cycle guard (forbidden_drop_targets()),
    // just for one already-known (item, target) pair instead of a
    // cursor-position-driven batch. No-op if item is already a member
    // of target, or target is item itself/one of its own descendants.
    void add_to_group(QGraphicsItem* item, GroupItem* target);
    // Whichever unlocked, drag_drop_enabled() group's area contains
    // `scenePos` - topmost by z if more than one candidate overlaps
    // there (e.g. a subgroup nested inside an outer group that also
    // qualifies), skipping anything in `excluded`. Shared by
    // maybe_add_dropped_items_to_group() (checked once, on release) and
    // mouseMoveEvent()'s live drop-target highlight (checked every
    // move, via GroupItem::set_highlighted()).
    GroupItem* find_drop_target_group(
        const QPointF& scenePos, const QSet<GroupItem*>& excluded = {}) const;
    // `draggedItems` plus, for every GroupItem among them, that group's
    // whole own subtree (itself + every nested descendant group,
    // recursively) - the set of groups that must NOT be offered as a
    // drop target for this drag, since accepting one would nest a group
    // inside itself or one of its own children.
    QSet<GroupItem*> forbidden_drop_targets(
        const QList<QGraphicsItem*>& draggedItems) const;
    // Raises `group`'s whole cluster (itself + every descendant,
    // recursively) to a fresh z band above everything else - same
    // sequential-band math as raise_selection_to_front(), just for an
    // explicit group rather than the current Qt selection. Not pushed
    // onto undo_stack_ - z-raises are a plain side effect throughout
    // this class, never their own undo step (see raise_selection_to_front()).
    void raise_group_cluster_to_front(GroupItem* group);
    void normalize_width_or_height(const QString& mode);
    void normalize_height();
    void normalize_width();
    void normalize_size();
    void arrange_default();
    void arrange(bool vertical = false);
    void arrange_optimal();
    void arrange_square();
    void flip_items(bool vertical = false);
    void crop_items();
    QColor sample_color_at(const QPointF& position);
    void select_all_items();
    void deselect_all_items();
    bool has_selection();
    bool has_single_selection();
    bool has_multi_selection();
    bool has_single_image_selection();
    // True if the selection is a single GroupItem, or a single item
    // that's currently a member of one (see find_owning_group()) - either
    // way, "Ungroup" has something to do.
    bool has_group_selected();

    // Wraps the current selection (2+ items) in a new GroupItem, pushed
    // as an undoable GroupCommand. No-op if fewer than 2 are selected.
    void group_selection();
    // Dissolves the selected GroupItem back into loose items, or - if a
    // single group MEMBER is selected instead - detaches just that one
    // item from its group, leaving the rest intact. No-op otherwise.
    void ungroup_selection();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

public:
    QList<QGraphicsItem*> selectedItems(bool userOnly = false) const;
    QList<QGraphicsItem*> items_by_type(const std::string& type);
    QList<QGraphicsItem*> items_for_save();
    void on_view_scale_change();
    QRectF itemsBoundingRect(bool selectionOnly = false,
                             QList<QGraphicsItem*> items
                             = QList<QGraphicsItem*>()) const;
    QPointF get_selection_center();

    // The scene's accumulated bounding rect, kept even once the scene is
    // emptied out again (unlike itemsBoundingRect(), which reflects only
    // the current items) - CanvasView::on_scene_changed() is the sole
    // writer during normal editing; FmlArchive reads/writes it verbatim
    // to/from manifest.json's "scene.boundingRect" so a saved project's
    // "remembered" empty space survives a save/reload round-trip. Empty
    // for a scene that's never had any content.
    QRectF rememberedBoundingRect() const { return rememberedBoundingRect_; }
    void setRememberedBoundingRect(const QRectF& rect)
    {
        rememberedBoundingRect_ = rect;
    }

public slots:
    void clear();
    void on_selection_change();
    void on_change();

public:
    void add_item_later(const QVariantMap& itemdata, bool selected = false);
    QList<IBaseItem*> add_queued_items();


    // Getter for active_mode_ (Python code just reads self.active_mode
    // directly; used e.g. by ItemMixin::on_selected_change()).
    ESceneMode active_mode() const;
    // Whether an item is a real user-facing one (pixmap/text/gif/group)
    // rather than a helper item (MultiSelectItem, RubberbandItem,
    // ErrorItem), based on IBaseItem::get_type()'s string tag rather
    // than a numeric type().
    bool itemAddByUser(QGraphicsItem* item) const;

    // Linear scan over items() for the one whose IBaseItem::uid()
    // matches - GroupItem::resolve_children() is the main caller (group
    // membership is tracked by uid, not a live pointer list - see
    // GroupItem's class comment in moveitem.h). No index/cache: fine at
    // the scale this app's boards actually run at, and avoids having to
    // keep a second data structure in sync with addItem()/removeItem().
    QGraphicsItem* find_by_uid(const QUuid& uid) const;
    // Scans every GroupItem in the scene for one whose child_ids()
    // contains memberUid - the only direction group membership is
    // tracked (group -> children), so "which group (if any) owns this
    // item" has no shortcut besides asking every group.
    GroupItem* find_owning_group(const QUuid& memberUid) const;
    // Every item (TextItem note, OR another PixmapItem/
    // GifItem) whose attachedToUid() is pictureUid - a picture
    // can have any number of things pinned to it, so this returns a
    // list, not a single match like find_owning_group(). Used by
    // PixmapItem::itemChange()
    // (moveitem.h) to carry them along when the picture moves, and by
    // deletion to cascade-remove them along with their anchor.
    QList<QGraphicsItem*> find_attached_items(const QUuid& pictureUid) const;
    // `items` plus every item attached (directly OR transitively, e.g.
    // C attached to B attached to A) to any picture already in `items`
    // (deduplicated) - CanvasView::on_action_delete_items()/
    // on_action_cut() use this so deleting a picture cascades to
    // whatever's pinned to it too, all as one
    // DeleteItemsCommand - one undo step restores everything.
    QList<QGraphicsItem*> with_attached_items(
        const QList<QGraphicsItem*>& items) const;
    // `items` deep-expanded: every GroupItem among them pulls in its
    // whole own subtree (descendants recursively, nested subgroups
    // included), AND every item (group or not) pulls in whatever's
    // attached to it (chains, recursively) - the general "select this,
    // act on everything it actually contains/carries" expansion shared
    // by copy_selection_to_internal_clipboard() and CanvasView::
    // on_action_delete_items()/on_action_cut(). Broader than
    // with_attached_items() above (that one only
    // walks the attach relationship, not group membership) - kept as a
    // separate method rather than folding into it since not every
    // caller wants the group expansion (e.g. raise_group_cluster_to_
    // front() explicitly wants ONLY one specific group's own cluster,
    // not every group nested inside `items`). Order preserving (a
    // QList, not a QSet) and always lists a GroupItem before its own
    // members - callers that re-stack z in list order (InsertItemsCommand::
    // redo()'s bring_to_front() loop) depend on that to keep a group's
    // fill behind its members.
    QList<QGraphicsItem*> with_related_items(
        const QList<QGraphicsItem*>& items) const;
    // True if attaching itemUid to targetUid would create a cycle in
    // the attachedToUid_ chain (itemUid == targetUid counts too - self-
    // attach is a 1-cycle). Walks UP from targetUid through its own
    // anchor, its anchor's anchor, etc.: if itemUid ever turns up there,
    // targetUid already (transitively) depends on itemUid, so attaching
    // itemUid TO targetUid would close the loop. Chains were impossible
    // before a picture could attach to another picture (Max) - a
    // TextItem note is always a leaf (nothing attaches to a note), so
    // this never mattered until now. Guards against the resulting
    // infinite recursion in PixmapItem::itemChange()/set_rotation()/
    // set_scale()'s own cascades, which walk find_attached_items()
    // recursively via ordinary virtual dispatch and have no cycle check
    // of their own - they rely on the graph being acyclic by
    // construction, which this is the one gate that guarantees.
    bool wouldCreateAttachCycle(const QUuid& itemUid,
                                const QUuid& targetUid) const;
    // Hierarchy panel drag-and-drop (current, PureRef-style interactive
    // tree): re-anchors item to targetUid (a PixmapItem/GifItem uid) as
    // one undo step, syncing item's group membership to match the
    // target's group so "an attached item always lives in the same
    // group as its anchor" keeps holding. No-op if targetUid doesn't
    // resolve to a picture, attaching would create a cycle
    // (wouldCreateAttachCycle()), or item is already attached there -
    // the tree UI is expected to not even offer an invalid drop in the
    // first place, this is just the defensive floor.
    void attach_item_to(QGraphicsItem* item, const QUuid& targetUid);
    // Hierarchy panel drag-and-drop (current) - the reverse: clears any
    // attachment AND leaves whatever group that attachment had folded
    // item into, landing it fully top-level. Also the
    // right call for dragging a plain (never-attached) grouped item, or
    // a nested subgroup, out to the tree's empty root area -
    // RemoveFromGroupCommand alone if there was no attachment to clear.
    // No-op if item is already fully top-level.
    void detach_item(QGraphicsItem* item);
    // Reentrant depth counter marking "some batch transform that
    // already gives its own attached notes an
    // independent, correctly-anchored move/rotate/scale is currently
    // applying" - covers two distinct sources: a group-level batch (a
    // plain group drag via GroupItem::itemChange()'s own children-
    // moveBy loop, or a group scale/rotate via selector.h's
    // mouseMoveEvent loop / Scale-RotateItemsByCommand's redo()/undo(),
    // both driven off GroupItem::selection_action_items()'s flat
    // expanded list - each member, including an attached note that's
    // ALSO a group member, gets its own list entry), and a LONE
    // picture's own set_rotation()/set_scale() (PixmapItem's own
    // overrides, moveitem.h - loop explicitly over that picture's
    // attached notes with the same delta/factor, group membership not
    // required). In both cases, PixmapItem::itemChange()'s simple
    // position-delta cascade to that SAME note would double-apply (or,
    // for scale/rotate, apply the wrong kind of change entirely) on top
    // of the already-correct transform - confirmed both for group
    // drag/resize and for a lone picture's own rotate. Depth (not a
    // bool) because nested groups cascade into their own itemChange()
    // recursively - each level's begin/end pair nests correctly. NOT
    // set for an individual picture dragged alone within its group (no
    // batch of any kind runs for a plain drag, only itemChange()'s own
    // moveBy chain) - that's exactly when the note DOES still need the
    // plain position cascade.
    void begin_group_batch() { ++groupBatchDepth_; }
    void end_group_batch() { --groupBatchDepth_; }
    bool in_group_batch() const { return groupBatchDepth_ > 0; }
    int groupBatchDepth_ = 0;
    // Whichever group is currently showing the live drop-target
    // highlight (mouseMoveEvent()) - tracked so the highlight can be
    // cleared off the PREVIOUS target when the cursor moves to a new
    // one, or off entirely on release/mode-cancel.
    GroupItem* highlightedGroup_ = nullptr;
    void clear_drop_target_highlight();

    QUndoStack* undo_stack_ = nullptr;
    qreal max_z = 0;
    qreal min_z = 0;
    qreal Z_STEP = 0.001;
    MultiSelectItem* multiselect_item_ = nullptr;
    RubberbandItem* rubberband_item_ = nullptr;
    std::queue<QueuedItemData> items_to_add;
    // Guards items_to_add: add_item_later() may be called from a
    // background ThreadedIO worker while add_queued_items() drains it
    // on the GUI thread.
    QMutex itemsToAddMutex_;
    // Shared (not per-tab) so copy on one tab's scene can be pasted into
    // another's - the "familiar/items" marker CanvasView::on_action_copy()
    // sets is on the system clipboard already, which is inherently
    // global; the actual items need to be too. Holding shared_ptr keeps
    // a copied item alive even if the scene it came from gets cleared/
    // closed before the paste happens.
    static inline QList<std::shared_ptr<IBaseItem>> internal_clipboard;
    TextItem* edit_item = nullptr;
    PixmapItem* crop_item = nullptr;
    QPointF event_start{};
    ESceneMode active_mode_{kNone};
    bool clear_ongoing = false;

    // ────────────────────────────────────────────────────────────────────────

    void pasteFromClipboard();
    void copyToClipboard();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);
    void setProjectSettings(project_settings* ps);
    void cleanupWorkplace();
    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();
    QUuid recoveryId();

public slots:
    void settingsChangedSlot();

private slots:
    void clipboardChanged();

private:
    qint16 objectsCount() const;

    void handleImageFromClipboard(const QImage& image);

    MainWindow& mainwindow_;
    uint64_t& zCounter_;

    qreal parentViewScaleFactor_ = 1;
    project_settings* projectSettings_;

    QRectF rememberedBoundingRect_;

    QPointF origin_;
    QRectF rubberBand_;
    QPointF lastClickedPoint_{0, 0};
    QColor selectionColor_;

    // Keeps every currently-attached item alive (shared with whichever
    // undo commands also reference it) for as long as it's actually in
    // the scene - see IBaseItem::acquireShared(). Keyed by the same
    // QGraphicsItem* passed to addItem()/removeItem(). Without this, an
    // item referenced by no command (e.g. its InsertItemsCommand got
    // trimmed off the undo stack by setUndoLimit() while the item is
    // still visible) would have nothing left to keep it alive.
    QHash<QGraphicsItem*, std::shared_ptr<IBaseItem>> attachedItems_;

    // Members of a locked group that a double-click drilled into (see
    // mouseDoubleClickEvent()) - their ItemIsSelectable/ItemIsMovable
    // flags were forced back on so they behave as ordinary items while
    // this lasts. Re-locked (flags turned back off) by
    // restore_drilled_in_members_() once they're no longer selected, so
    // re-selecting them again requires another explicit double-click.
    QList<QGraphicsItem*> drilledInMembers_;
    void restore_drilled_in_members_();
};

#endif // CANVASSCENE_H

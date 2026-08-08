#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QMutex>
#include <QRubberBand>
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
    // Drag-and-drop-to-add (roadmap step 10 stage 5) - called from
    // mouseReleaseEvent() after committing a body drag, with the
    // cursor's OWN scene position (not any dragged item's center/bounds
    // - matches normal drag-and-drop expectations: wherever you're
    // actually pointing at release is the target, Max's explicit spec).
    // Every LOOSE item among movedItems (not already a member of any
    // group, and not a group itself - dropping one group onto another is
    // the "select both + Ctrl+G" nesting flow, not this) becomes a new
    // member of whichever unlocked, drag_drop_enabled() group's area
    // contains the cursor - topmost by z if more than one candidate
    // overlaps there (e.g. a subgroup nested inside an outer group that
    // also qualifies).
    void maybe_add_dropped_items_to_group(const QList<QGraphicsItem*>& movedItems,
                                          const QPointF& cursorScenePos);
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
    // way, "Ungroup" has something to do (roadmap step 10).
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

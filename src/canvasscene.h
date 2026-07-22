#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include "core/settings.h"
#include "image_downloader.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMutex>
#include <QRubberBand>
#include <QVariantMap>

#include <queue>

class MainWindow;
class project_settings;
class TextItem;
class PixmapItem;
class RubberbandItem;
class MultiSelectItem;
class QUndoStack;
class IBaseItem;
class FamSettings;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT
public:
    enum ESceneMode
    {
        kNone = 0,
        kMoveMode = 1,
        kRubberbandMode = 2,
    };

signals:
    void cursor_changed(QCursor);
    void cursor_cleared();

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
    void cancel_active_modes();
    void end_rubberband_mode();
    void cancel_crop_mode();
    void copy_selection_to_internal_clipboard();
    void paste_from_internal_clipboard(QPointF position);
    void raise_to_top();
    void lower_to_bottom();
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

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

public:
    QList<QGraphicsItem*> selectedItems(bool userOnly = false) const;
    QList<QGraphicsItem*> items_by_type(int type);
    QList<QGraphicsItem*> items_for_save();
    void clear_save_ids();
    void on_view_scale_change();
    QRectF itemsBoundingRect(bool selectionOnly = false,
                             QList<QGraphicsItem*> items
                             = QList<QGraphicsItem*>()) const;
    QPointF get_selection_center();

public slots:
    void clear();
    void on_selection_changed();
    void on_change();

public:
    void add_item_later(const QVariantMap& itemdata, bool selected = false);
    QList<IBaseItem*> add_queued_items();


    // Getter for active_mode_ (Python code just reads self.active_mode
    // directly; used e.g. by ItemMixin::on_selected_change()).
    ESceneMode active_mode() const;
    // Selects/deselects every item in the scene (Python only has
    // select_all_items(), which uses setSelectionArea()).
    void set_selected_all_items(bool value);
    // Stand-in for Python's hasattr(item, 'save_id') duck-typing check:
    // whether an item is a real user-facing one (pixmap/text), based on
    // IBaseItem::get_type()'s string tag rather than a numeric type().
    bool itemAddByUser(QGraphicsItem* item) const;

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
    QList<IBaseItem*> internal_clipboard;
    TextItem* edit_item = nullptr;
    PixmapItem* crop_item = nullptr;
    QPointF event_start{};
    ESceneMode active_mode_{kNone};
    bool clear_ongoing = false;

    // ────────────────────────────────────────────────────────────────────────

    void pasteFromClipboard();
    void copyToClipboard();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);
    QByteArray fml_payload();
    void setProjectSettings(project_settings* ps);
    void cleanupWorkplace();
    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

public slots:
    void slotMove(QGraphicsItem* signalOwner, qreal dx, qreal dy);
    void settingsChangedSlot();

private slots:
    void clipboardChanged();

private:
    qint16 objectsCount() const;
    bool isAnySelectedUnderCursor() const;
    void deselectItems();

    void handleImageFromClipboard(const QImage& image);
    void handleHtmlFromClipboard(const QString& html);

    MainWindow& mainwindow_;
    uint64_t& zCounter_;

    ImageDownloader* imgdownloader_ = nullptr; //image loader need c++threads!!!

    qreal parentViewScaleFactor_ = 1;
    project_settings* projectSettings_;

    QPointF origin_;
    QRectF rubberBand_;
    QPointF lastClickedPoint_{0, 0};
    QColor selectionColor_;
    FamSettings* settings{nullptr};
};

#endif // CANVASSCENE_H

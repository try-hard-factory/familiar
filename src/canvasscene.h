#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include "image_downloader.h"
#include "mainselectedarea.h"
#include <itemgroup.h>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QRubberBand>
#include <QVariantMap>

#include <queue>

enum EState {
    eMouseMoving = 0x0000,
    eMouseSelection = 0x0001,
    eGroupItemMoving = 0x0002,
    eGroupItemResizing = 0x0003,
};

class MainWindow;
class project_settings;
class TextItem;
class PixmapItem;
class RubberbandItem;
class MultiSelectItem;
class QUndoStack;
class IBaseItem;

class CanvasScene : public QGraphicsScene
{
public:
    CanvasScene(MainWindow& mw,
                uint64_t& zc,
                QUndoStack* undoStack,
                QGraphicsScene* scene = 0);
    ~CanvasScene();

    void clear();
    void cancel_active_modes();
    // new code BEGIN
    void addItem(QGraphicsItem* item);
    void removeItem(QGraphicsItem* item);
    void cancel_crop_mode();
    void end_rubberband_mode();
    void copy_selection_to_internal_clipboard();
    void paste_from_internal_clipboard(QPointF position);
    void raise_to_top();
    void lower_to_bottom();
    void normalize_width_or_height(const QString& mode);
    void normalize_height();
    void normalize_width();
    void normalize_size();
    void arrange(bool vertical = false);
    void arrange_optimal();
    void flip_items(bool vertical = false);
    void crop_items();
    void set_selected_all_items(bool value);
    bool has_selection();
    bool has_single_selection();
    bool has_multi_selection();
    bool has_croppable_selection();

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
    void on_selection_changed();
    void on_change();


signals:
    void cursor_changed(QCursor);
    void cursor_cleared();

public:
    // Structure to hold item data for queued items
    struct QueuedItemData {
        QVariantMap data;
        bool selected = false;
    };

    void add_item_later(const QVariantMap& itemdata, bool selected = false);
    void add_queued_items();

    
    bool itemAddByUser(int type) const;


    // old code
    void pasteFromClipboard();
    void copyToClipboard();
    void onSelectionChanged();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);
    QByteArray fml_payload();
    // void updateViewScaleFactor(qreal newval)
    // {
    //     itemGroup_->setScaleControlFactor(newval);
    //     parentViewScaleFactor_ = newval;
    // }

    void setProjectSettings(project_settings* ps);

    void cleanupWorkplace();

    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();
    // ItemGroup* itemGroup() const noexcept { return itemGroup_; }

    void onSelectionChange()
    {
        // if (hasMultiSelection())
        //     multiSelectItem->fitSelectionArea(itemsBoundingRect(true));
        // if (hasMultiSelection() && !multiSelectItem->scene()) {
        //     addItem(multiSelectItem);
        //     multiSelectItem->bringToFront();
        // }
        // if (!hasMultiSelection() && multiSelectItem->scene())
        //     removeItem(multiSelectItem);
    }


protected:
    void keyPressEvent(QKeyEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

public slots:
    void slotMove(QGraphicsItem* signalOwner, qreal dx, qreal dy);
    void settingsChangedSlot();

private slots:
    void clipboardChanged();


public:
    // new code BEGIN
    bool move_active = false;
    bool rubberband_active = false;
    QUndoStack* undo_stack_ = nullptr;
    qreal max_z = 0;
    qreal min_z = 0;
    qreal Z_STEP = 0.001;
    MultiSelectItem* multiselect_item_;
    RubberbandItem* rubberband_item_;
    std::queue<QueuedItemData> items_to_add;
    QList<IBaseItem*> internal_clipboard;
    TextItem* edit_item = nullptr;
    PixmapItem* crop_item = nullptr;
    // new code END

    bool clear_ongoing = false;
    QPointF event_start{};

private:
    qint16 objectsCount() const;
    bool isAnySelectedUnderCursor() const;
    void deselectItems();

    void handleImageFromClipboard(const QImage& image);
    void handleHtmlFromClipboard(const QString& html);

    MainWindow& mainwindow_;
    uint64_t& zCounter_;
    // ItemGroup* itemGroup_ = nullptr;
    MainSelectedArea mainSelArea_;

    EState state_ = eMouseMoving;

    ImageDownloader* imgdownloader_ = nullptr; //image loader need c++threads!!!

    qreal parentViewScaleFactor_ = 1;
    project_settings* projectSettings_;

    QPointF origin_;
    QRectF rubberBand_;
    QPointF lastClickedPoint_{0, 0};
    QColor selectionColor_;
};

#endif // CANVASSCENE_H

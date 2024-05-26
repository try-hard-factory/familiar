#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include "image_downloader.h"
#include "mainselectedarea.h"
#include <itemgroup.h>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QRubberBand>
//#include "moveitem.h"
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
class RubberBandItem;
class MultiSelectItem;
class QUndoStack;

class CanvasScene : public QGraphicsScene
{
    enum ESceneMode {
        kMove = 1,
        kRubberBand = 2,
        kNone = -1,
    };

public:
    CanvasScene(MainWindow& mw, uint64_t& zc, QUndoStack* undoStack, QGraphicsScene* scene = 0);
    ~CanvasScene();

// new code
    void clear();
    bool has_multi_selection();
    void addItem(QGraphicsItem* item);
    void removeItem(QGraphicsItem* item);
    void cancel_active_modes();
    void cancel_crop_mode();
    void end_rubberband_mode();
    void copy_selection_to_internal_clipboard();
    void paste_from_internal_clipboard(QPointF position);
    void raise_to_top();
    void lower_to_bottom();
    void normalize_width_or_height(const QString& mode);
    void normalize_height();
    void normalize_width();
// old code
    void pasteFromClipboard();
    void pasteFromTemp();
    void copyToClipboard();
    void onSelectionChanged();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);
    void addImageToSceneToPosition(QImage&& image, QPointF position);
    QByteArray fml_payload();
    void updateViewScaleFactor(qreal newval)
    {
        itemGroup_->setScaleControlFactor(newval);
        parentViewScaleFactor_ = newval;
    }

    void setProjectSettings(project_settings* ps);

    void cleanupWorkplace();

    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();
    ItemGroup* itemGroup() const noexcept { return itemGroup_; }

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
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;

public slots:
    void slotMove(QGraphicsItem* signalOwner, qreal dx, qreal dy);
    void settingsChangedSlot();
    void on_selection_changed();
    void on_change();
private slots:
    void clipboardChanged();

signals:
    void cursor_changed(QCursor);
    void cursor_cleared();

public:    
    ESceneMode active_mode = kNone;
    QUndoStack* undo_stack_ = nullptr;
    qreal max_z = 0;
    qreal min_z = 0;
    qreal Z_STEP = 0.001;
    TextItem* edit_item = nullptr;
    PixmapItem* crop_item = nullptr;
    std::queue<PixmapItem*> items_to_add;
    bool clear_ongoing = false;
    RubberBandItem* rubberband_item_;
    MultiSelectItem* multiselect_item_;
    QList<QGraphicsItem*> internal_clipboard;
private:
    qint16 objectsCount() const;
    bool isAnySelectedUnderCursor() const;
    void deselectItems();

    void handleImageFromClipboard(const QImage& image);
    void handleHtmlFromClipboard(const QString& html);

    MainWindow& mainwindow_;
    uint64_t& zCounter_;
    ItemGroup* itemGroup_ = nullptr;
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

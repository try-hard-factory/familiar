#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <itemgroup.h>
#include "mainselectedarea.h"
#include "image_downloader.h"

enum EState {
    eMouseMoving          = 0x0000,
    eMouseSelection       = 0x0001,
    eGroupItemMoving      = 0x0002,
    eGroupItemResizing    = 0x0003,
};

class CanvasScene : public QGraphicsScene
{
public:
    CanvasScene(uint64_t& zc, QGraphicsScene *scene = 0);
    ~CanvasScene() = default;

public:
    void pasteFromClipboard();
    void onSelectionChanged();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);
    void addImageToSceneToPosition(QImage&& image, QPointF position);
    std::string fml_header();
    QByteArray fml_payload();
    void updateViewScaleFactor(qreal newval) {
        itemGroup_->setScaleControlFactor(newval);
        parentViewScaleFactor_ = newval;
    }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;


public slots:
    void slotMove(QGraphicsItem *signalOwner, qreal dx, qreal dy);

private:
    bool isAnySelectedUnderCursor() const;
    void deselectItems();

    void handleImageFromClipboard(const QImage& image);
    void handleHtmlFromClipboard(const QString& html);

    uint64_t& zCounter_;
    ItemGroup* itemGroup_ = nullptr;
    MainSelectedArea mainSelArea_;


    EState state_ = eMouseMoving;

    ImageDownloader* imgdownloader_ = nullptr;

    qreal parentViewScaleFactor_ = 1;
};

#endif // CANVASSCENE_H

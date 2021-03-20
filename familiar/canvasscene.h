#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <itemgroup.h>
#include "mainselectedarea.h"

class CanvasScene : public QGraphicsScene
{
public:
    CanvasScene(uint64_t& zc, QGraphicsScene *scene = 0);
    ~CanvasScene() = default;

public:
    void onSelectionChanged();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    bool isAnySelectedUnderCursor() const;

    uint64_t& zCounter_;
    ItemGroup* itemGroup_ = nullptr;
    MainSelectedArea mainSelArea_;
};

#endif // CANVASSCENE_H

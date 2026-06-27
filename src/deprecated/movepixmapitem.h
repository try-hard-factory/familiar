#ifndef MOVEPIXMAPITEM_H
#define MOVEPIXMAPITEM_H
#include <QCursor>
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include <QPainter>
#include <QWheelEvent>

class MovePixmapItem : public QGraphicsPixmapItem
{
public:
    explicit MovePixmapItem(QGraphicsItem* parent = 0);
    ~MovePixmapItem();

protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void wheelEvent(QGraphicsSceneWheelEvent* event) override;

private:
    QPointF shiftMouseCoords_;
    QPixmap pixmap_;
};

#endif // MOVEPIXMAPITEM_H

#ifndef MOVEITEM_H
#define MOVEITEM_H

#include <QObject>
#include <QWheelEvent>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QCursor>

class MovableCircle : public QGraphicsObject
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    enum ECirclePos {
        eTopLeft = 0,
        eTopRight,
        eBottomRight,
        eBottomLeft,
    };

    explicit MovableCircle(ECirclePos cp, QGraphicsItem *parent = 0);

private:
    QRectF boundingRect() const;
    QPainterPath shape() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    QPointF _shiftMouseCoords;

private:
    ECirclePos circlePos_;
signals:
    void circleMoved();
};

class MoveItem : public QObject, public QGraphicsItem//public QGraphicsObject
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MoveItem(const QString& path, uint64_t& zc, QGraphicsItem *parent = nullptr);
    MoveItem(const QImage& img, uint64_t& zc, QGraphicsItem *parent = nullptr);
    ~MoveItem();
    QRectF getRect() const { return boundingRect(); }
signals:
protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QPointF shiftMouseCoords_;
    QImage qimage_;
    QPixmap pixmap_;
    uint64_t& zCounter_;
    MovableCircle *_topLeftCircle, *_topRightCircle, *_bottomLeftCircle, *_bottomRightCircle;
    QSizeF _size;
    QRectF rect_;
public slots:
};

#endif // MOVEITEM_H

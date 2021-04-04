#ifndef BORDERDOT_H
#define BORDERDOT_H

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QCursor>

class BorderDot : public QGraphicsObject
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

    explicit BorderDot(ECirclePos cp, QGraphicsItem *parent = 0);

    enum { Type = UserType + 1 };

    int type() const override
    {
        return Type;
    }

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

#endif // BORDERDOT_H

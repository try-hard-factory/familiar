#ifndef ITEMGROUP_H
#define ITEMGROUP_H

#include <QObject>
#include <QGraphicsItemGroup>

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


class ItemGroup : public QObject, public QGraphicsItemGroup
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    enum EItemsType {
        eBorderDot = QGraphicsItem::UserType + 1,
    };
    ItemGroup(uint64_t& zc, QGraphicsItemGroup *parent = nullptr);
public:
    void addItem(QGraphicsItem* item);
    void printChilds();

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

    void clearItemGroup();
    bool isContain(const QGraphicsItem* item) const;
    bool isEmpty() const;
    void incZ();
protected:
//    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void MovableCirclesSetVisible(bool visibility);

private:
    QPointF shiftMouseCoords_;
    uint64_t& zCounter_;
    MovableCircle *_topLeftCircle, *_topRightCircle, *_bottomLeftCircle, *_bottomRightCircle;
};

#endif // ITEMGROUP_H

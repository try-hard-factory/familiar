#ifndef ITEMGROUP_H
#define ITEMGROUP_H

#include <QGraphicsItemGroup>

class ItemGroup : public QGraphicsItemGroup
{
public:
    ItemGroup(uint64_t& zc);
public:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void clearItemGroup();
    bool isContain(const QGraphicsItem* item) const;
    bool isEmpty() const;
    void incZ();
protected:
//    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPointF shiftMouseCoords_;
    uint64_t& zCounter_;
};

#endif // ITEMGROUP_H

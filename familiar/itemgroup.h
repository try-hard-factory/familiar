#ifndef ITEMGROUP_H
#define ITEMGROUP_H

#include <QGraphicsItemGroup>

class ItemGroup : public QGraphicsItemGroup
{
public:
    ItemGroup(uint64_t& zc);
public:

    enum ActionStates {
        ResizeState = 0x01,
        RotationState = 0x02
    };
    enum CornerFlags {
        Top = 0x01,
        Bottom = 0x02,
        Left = 0x04,
        Right = 0x08,
        TopLeft = Top|Left,
        TopRight = Top|Right,
        BottomLeft = Bottom|Left,
        BottomRight = Bottom|Right
    };

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

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void resizeLeft( const QPointF &pt);
    void resizeRight( const QPointF &pt);
    void resizeBottom(const QPointF &pt);
    void resizeTop(const QPointF &pt);

private:
    QPointF shiftMouseCoords_;
    uint64_t& zCounter_;
    unsigned int cornerFlags_;
    unsigned int actionFlags_;
};

#endif // ITEMGROUP_H

#ifndef ITEMGROUP_H
#define ITEMGROUP_H

#include <QObject>
#include <QGraphicsItemGroup>


class DotSignal;
class QGraphicsSceneMouseEvent;

class ItemGroup : public QObject, public QGraphicsItemGroup
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
    Q_PROPERTY(QPointF previousPosition READ previousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)
public:
    enum EItemsType {
        eBorderDot = QGraphicsItem::UserType + 1,
    };
    ItemGroup(uint64_t& zc, QGraphicsItemGroup *parent = nullptr);
    ~ItemGroup();
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

    enum CornerGrabbers {
        GrabberTop = 0,
        GrabberBottom,
        GrabberLeft,
        GrabberRight,
        GrabberTopLeft,
        GrabberTopRight,
        GrabberBottomLeft,
        GrabberBottomRight
    };

public:
    void addItem(QGraphicsItem* item);
    void printChilds();
    QPointF previousPosition() const;
    void setPreviousPosition(const QPointF previousPosition);


signals:
    void rectChanged(ItemGroup *rect);
    void previousPositionChanged();
    void clicked(ItemGroup *rect);
    void signalMove(QGraphicsItemGroup *item, qreal dx, qreal dy);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
public:
    void clearItemGroup();
    bool isContain(const QGraphicsItem* item) const;
    bool isEmpty() const;
    void incZ();
protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;



private:
    QPointF shiftMouseCoords_;
    uint64_t& zCounter_;
    QRectF m_tmpRect;

private:
    unsigned int m_cornerFlags;
    unsigned int m_actionFlags;
    QPointF m_previousPosition;
    bool m_leftMouseButtonPressed;
    DotSignal *cornerGrabber[8];

    void resizeLeft( const QPointF &pt);
    void resizeRight( const QPointF &pt);
    void resizeBottom(const QPointF &pt);
    void resizeTop(const QPointF &pt);

    void rotateItem(const QPointF &pt);
    void setPositionGrabbers();
    void setVisibilityGrabbers();
    void hideGrabbers();
};

#endif // ITEMGROUP_H

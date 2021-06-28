#ifndef ITEMGROUP_H
#define ITEMGROUP_H

#include <QObject>
#include <QGraphicsItemGroup>

class MoveItem;
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

    struct new_size_t {
        qreal new_x = .0;
        qreal new_y = .0;
        qreal new_w = .0;
        qreal new_h = .0;
    };
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
        GrabberTopLeft = 0,
        GrabberTopRight,
        GrabberBottomLeft,
        GrabberBottomRight
    };

public:
    void addItemToGroup(QGraphicsItem* item);
    void removeItemFromGroup(QGraphicsItem* item);
    void printChilds();
    QPointF previousPosition() const;
    void setPreviousPosition(const QPointF previousPosition);
    QRectF boundingRect() const override;

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
    bool isThisDots(const QGraphicsItem *item) const;
    bool isEmpty() const;
    void incZ();
    QRectF realRect() const {return rectItemGroup_;}

    void dumpBits(QString text);
protected:


    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF currentSceneBoundingRect() const;
    void printDotsCoords(const std::string& text) const;

    new_size_t calculateNewSize(const QRectF& tmpRect, MoveItem* widget);
    QRectF calcNewBr();
private:
    QPointF shiftMouseCoords_;
    uint64_t& zCounter_;
    QRectF sceneRectItemGroup_;
    QRectF rectItemGroup_;
    QVector<QGraphicsItem*> items_;

private:
    unsigned int m_cornerFlags;
    unsigned int m_actionFlags;
    QPointF m_previousPosition;
    bool m_leftMouseButtonPressed;
    DotSignal *cornerGrabber[4] = {nullptr, nullptr, nullptr, nullptr};

    void resizeTopLeft(const QPointF &pt);
    void resizeTopRight(const QPointF &pt);
    void resizeBottomLeft(const QPointF &pt);
    void resizeBottomRight(const QPointF &pt);

    void rotateItem(const QPointF &pt);
    void setPositionGrabbers();
    void setVisibilityGrabbers();
    void hideGrabbers();
};

#endif // ITEMGROUP_H

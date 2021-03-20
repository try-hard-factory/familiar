#include "itemgroup.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>

#include "Logger.h"

extern Logger logger;


#define MOUSE_MOVE_DEBUG
ItemGroup::ItemGroup(uint64_t& zc) : zCounter_(zc), m_cornerFlags(0)
{
    setAcceptHoverEvents(true);
}


void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
#endif
    this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
}

void ItemGroup::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(++zCounter_);
    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();

    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
}

void ItemGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
//    qInfo()<<"MoveItem::mouseReleaseEvent curent scale "<<this->scale()\
//          <<", Event->pos: "<<event->pos()
//         <<", Pos: "<<pos()
//         <<", Shift"<<shiftMouseCoords_;
    //    qInfo() <<'\n';
}

void ItemGroup::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{

}

void ItemGroup::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_cornerFlags = 0;
    QGraphicsItem::hoverLeaveEvent( event );
}

void ItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - boundingRect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - boundingRect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - boundingRect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - boundingRect().bottom();   // Distance between the mouse and the bottom

    LOG_WARNING(logger, drx, " ", dlx, " ", dby, " ", dty);
    // If the mouse position is within a radius of 7
    // to a certain side( top, left, bottom or right)
    // we set the Flag in the Corner Flags Register

    m_cornerFlags = 0;
    if( dby < 20 && dby > -20 ) m_cornerFlags |= Top;       // Top side
    if( dty < 20 && dty > -20 ) m_cornerFlags |= Bottom;    // Bottom side
    if( drx < 20 && drx > -20 ) m_cornerFlags |= Right;     // Right side
    if( dlx < 20 && dlx > -20 ) m_cornerFlags |= Left;      // Left side

    switch (m_cornerFlags) {
    case TopLeft:
    case TopRight:
    case BottomLeft:
    case BottomRight: {
        setCursor(Qt::BusyCursor);
        break;
    }
    default:
        setCursor(Qt::CrossCursor);
        break;
    }
}

void ItemGroup::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
//    LOG_WARNING(logger, "!");
//    auto childs = childItems();
//    if (childs.empty()) return;
//    qInfo()<<"ItemGroup::paint";
//    painter->save();
//    int wsize = 2;
//    painter->setPen( QPen(Qt::red, wsize) );
//    auto br = this->boundingRect().bottomRight();
//    QPointF tl(-1, -1);
////    br.rx() += wsize;
////    br.ry() += wsize;
//    QRectF r(tl, br);
//    painter->drawRect(r);
//    painter->restore();
}

void ItemGroup::clearItemGroup()
{
    auto childs = childItems();
    for (auto& it : childs) {
        LOG_DEBUG(logger, "REMOVE ", it);
        removeFromGroup(it);
    }
}

bool ItemGroup::isContain(const QGraphicsItem *item) const
{
    if (this == item->parentItem()) return true;

    return false;
}

bool ItemGroup::isEmpty() const
{
    auto childs = childItems();
    return childs.empty();
}

void ItemGroup::incZ()
{
    setZValue(++zCounter_);
}

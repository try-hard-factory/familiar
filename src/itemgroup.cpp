#include "itemgroup.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>

#include "Logger.h"

extern Logger logger;


#define MOUSE_MOVE_DEBUG
ItemGroup::ItemGroup(uint64_t& zc) :
    zCounter_(zc),
    cornerFlags_(0),
    actionFlags_(ResizeState)

{
    setAcceptHoverEvents(true);
}


void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
#endif
    QPointF pt = event->pos();
    if(actionFlags_ == ResizeState){
        switch (cornerFlags_) {
        case Top:
            resizeTop(pt);
            break;
        case Bottom:
            resizeBottom(pt);
            break;
        case Left:
            resizeLeft(pt);
            break;
        case Right:
            resizeRight(pt);
            break;
        case TopLeft:
            resizeTop(pt);
            resizeLeft(pt);
            break;
        case TopRight:
            resizeTop(pt);
            resizeRight(pt);
            break;
        case BottomLeft:
            resizeBottom(pt);
            resizeLeft(pt);
            break;
        case BottomRight:
            resizeBottom(pt);
            resizeRight(pt);
            break;
        default:
            this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
            break;
        }
    }
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
    cornerFlags_ = 0;
    QGraphicsItem::hoverLeaveEvent( event );
}

void ItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - boundingRect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - boundingRect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - boundingRect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - boundingRect().bottom();   // Distance between the mouse and the bottom

//    LOG_WARNING(logger, drx, " ", dlx, " ", dby, " ", dty);
    // If the mouse position is within a radius of 7
    // to a certain side( top, left, bottom or right)
    // we set the Flag in the Corner Flags Register

    cornerFlags_ = 0;
    if( dby < 10 && dby > -10 ) cornerFlags_ |= Top;       // Top side
    if( dty < 10 && dty > -10 ) cornerFlags_ |= Bottom;    // Bottom side
    if( drx < 10 && drx > -10 ) cornerFlags_ |= Right;     // Right side
    if( dlx < 10 && dlx > -10 ) cornerFlags_ |= Left;      // Left side

    switch (cornerFlags_) {
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


void ItemGroup::resizeLeft(const QPointF &pt)
{
    LOG_WARNING(logger,"");
    QRectF tmpRect = boundingRect();
    // if the mouse is on the right side we return
    if( pt.x() > tmpRect.right() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.right() );
    // limit the minimum width
    if( widthOffset > -10 )
        return;
    // if it's negative we set it to a positive width value
    if( widthOffset < 0 )
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    // Since it's a left side , the rectange will increase in size
    // but keeps the topLeft as it was
    tmpRect.translate( boundingRect().width() - tmpRect.width() , 0 );
    prepareGeometryChange();
    // Update to see the result
    update();
}

void ItemGroup::resizeRight(const QPointF &pt)
{
    LOG_WARNING(logger,"");
    QRectF tmpRect = boundingRect();
    if( pt.x() < tmpRect.left() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 10)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    prepareGeometryChange();
    update();
}

void ItemGroup::resizeBottom(const QPointF &pt)
{
    LOG_WARNING(logger,"");
    QRectF tmpRect = boundingRect();
    if( pt.y() < tmpRect.top() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.top() );
    if( heightOffset < 11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    prepareGeometryChange();
    update();
}

void ItemGroup::resizeTop(const QPointF &pt)
{
    LOG_WARNING(logger,"");
    QRectF tmpRect = boundingRect();
    if( pt.y() > tmpRect.bottom() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.bottom() );
    if( heightOffset > -11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    tmpRect.translate( 0 , boundingRect().height() - tmpRect.height() );
    prepareGeometryChange();
    update();
}

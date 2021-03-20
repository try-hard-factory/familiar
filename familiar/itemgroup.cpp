#include "itemgroup.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include "Logger.h"

extern Logger logger;


#define MOUSE_MOVE_DEBUG
ItemGroup::ItemGroup(uint64_t& zc) : zCounter_(zc)
{

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

void ItemGroup::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
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

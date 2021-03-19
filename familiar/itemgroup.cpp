#include "itemgroup.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#define MOUSE_MOVE_DEBUG
ItemGroup::ItemGroup(uint64_t& zc) : zCounter_(zc)
{

}

void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    qInfo()<<"ItemGroup::mouseMoveEvent curent scale "<<this->scale()
          << ", EventPos: "<< event->pos()
          << ", Pos: "<<pos();
#endif
    this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
}

void ItemGroup::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(++zCounter_);
    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();

    qInfo()<<"ItemGroup::mousePressEvent curent scale "<<this->scale()
          <<", Event->pos: "<<event->pos()
         <<", Pos: "<<pos()
         <<", Shift"<<shiftMouseCoords_;
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

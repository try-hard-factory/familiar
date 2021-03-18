#include "moveitem.h"

MoveItem::MoveItem(uint64_t& zc, QObject *parent) :
    QObject(parent), QGraphicsItem(), zCounter_(zc)
{
    auto qimage = QImage("kot.png");
    qInfo() << qimage.width() << ' ' <<qimage.height();
    pixmap_ = QPixmap::fromImage(qimage);
    setAcceptHoverEvents(true);
}

MoveItem::~MoveItem()
{

}


QRectF MoveItem::boundingRect() const
{
    return QRectF (0,0,pixmap_.width(),pixmap_.height());
//    return QRectF (0,0,100,100);
}

void MoveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::red);
//    painter->setBrush(Qt::green);
    painter->drawRect(-1,-1,pixmap_.width()+1,pixmap_.height()+1);
    QPointF point(0, 0);
    QRectF source(0, 0, pixmap_.width(),pixmap_.height());
    painter->drawPixmap(point, pixmap_, source);
    Q_UNUSED(option);
    Q_UNUSED(widget);
}

void MoveItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo()<<"MoveItem::mouseMoveEvent curent scale "<<this->scale()
          << ", EventPos: "<< event->pos()
          << ", Pos: "<<pos();

    /* Устанавливаем позицию графического элемента
     * в графической сцене, транслировав координаты
     * курсора внутри графического элемента
     * в координатную систему графической сцены
     * */
    this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
}

void MoveItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(++zCounter_);
    qInfo()<<"MoveItem::mousePressEvent curent scale "<<this->scale()\
          <<", Event->pos: "<<event->pos()
         <<", Pos: "<<pos()
         <<", Shift"<<shiftMouseCoords_;

    /* При нажатии мышью на графический элемент
     * заменяем курсор на руку, которая держит этот элемента
     * */
    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();
    this->setCursor(QCursor(Qt::ClosedHandCursor));
    Q_UNUSED(event);
}

void MoveItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo()<<"MoveItem::mouseReleaseEvent curent scale "<<this->scale()\
          <<", Event->pos: "<<event->pos()
         <<", Pos: "<<pos()
         <<", Shift"<<shiftMouseCoords_;

    /* При отпускании мышью элемента
     * заменяем на обычный курсор стрелку
     * */

    this->setCursor(QCursor(Qt::ArrowCursor));
    Q_UNUSED(event);
}

void MoveItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    qInfo()<<"MoveItem::wheelEvent curent scale "<<this->scale();
    qInfo()<<event->delta();
    /*Scale 0.2 each turn of the wheel (which is usually 120.0 eights of degrees)*/
    qreal scaleFactor = 1.15;//1.0 + event->delta() * 0.2 / 120.0;
    if (event->delta()>0)
        setScale(scale() * scaleFactor);
    else
        setScale(scale() * (1/scaleFactor));
//    setTransformOriginPoint(mapFromScene(event->scenePos()));

}

void MoveItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
//  qInfo()<<"MoveItem::hoverEnterEvent";
}

void MoveItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
//  qInfo()<<"MoveItem::hoverLeaveEvent";
}

void MoveItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
//  qInfo()<<"MoveItem::hoverMoveEvent";
}

#include "moveitem.h"
#include <QPen>

//#define MOUSE_MOVE_DEBUG

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
}

void MoveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPointF point(0, 0);
    QRectF source(0, 0, pixmap_.width(),pixmap_.height());
    painter->drawPixmap(point, pixmap_, source);

    if (option->state & QStyle::State_Selected) {
        QPen p;
        int wsize = 2;
        p.setWidth(wsize);
        p.setColor(QColor(0, 160, 230));
        painter->setPen(p);
        painter->drawRect(-1,-1,pixmap_.width()+wsize,pixmap_.height()+wsize);
    }

    Q_UNUSED(widget);
}

void MoveItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    qInfo()<<"MoveItem::mouseMoveEvent curent scale "<<this->scale()
          << ", EventPos: "<< event->pos()
          << ", Pos: "<<pos();
#endif
    this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
}

void MoveItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(++zCounter_);
    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();

    qInfo()<<"MoveItem::mousePressEvent curent scale "<<this->scale()
          <<", Event->pos: "<<event->pos()
         <<", Pos: "<<pos()
         <<", Shift"<<shiftMouseCoords_;

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
            qDebug()<<"MoveItem::mousePressEvent. Left Mouse and Shift";
//            setSelected(!isSelected());
        } else {
            QGraphicsItem::mousePressEvent(event);
        }
    }
}

void MoveItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo()<<"MoveItem::mouseReleaseEvent curent scale "<<this->scale()\
          <<", Event->pos: "<<event->pos()
         <<", Pos: "<<pos()
         <<", Shift"<<shiftMouseCoords_;

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
            qDebug()<<"MoveItem::mousePressEvent. Left Mouse and Shift";
            setSelected(!isSelected());
        } else {
            QGraphicsItem::mousePressEvent(event);
        }
    }
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

#include "debug_macros.h"
#include "moveitem.h"
#include <QPen>


#include "Vec2d.h"
#include "Logger.h"

extern Logger logger;

#define MOUSE_MOVE_DEBUG


/*********************************************************************/


MoveItem::MoveItem(const QString& path, uint64_t& zc, QGraphicsRectItem *parent) :
    QGraphicsRectItem(parent), zCounter_(zc)
{
    setZValue(zCounter_);
//    qimage_ = QImage("bender.png");
    qimage_ = QImage(path);
//    auto qimage = QImage("kot.jpg");
//    qInfo() << qimage.width() << ' ' <<qimage.height();
    pixmap_ = QPixmap::fromImage(qimage_);
    _size = pixmap_.size();
    rect_ = qimage_.rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

MoveItem::MoveItem(const QImage& img, uint64_t& zc, QGraphicsRectItem *parent) :
    QGraphicsRectItem(parent), qimage_(img), zCounter_(zc)
{
    setZValue(zCounter_);
    pixmap_ = QPixmap::fromImage(qimage_);
    _size = pixmap_.size();
    rect_ = qimage_.rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

MoveItem::~MoveItem()
{

}


QRectF MoveItem::boundingRect() const
{
//    qDebug()<<rect_;
    return QRectF (0, 0, rect_.width(), rect_.height());//pixmap_.width(),pixmap_.height());
}

void MoveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPointF point(0, 0);
    QRectF source(0, 0, pixmap_.width(),pixmap_.height());
     painter->drawImage(boundingRect(), qimage_);
//    painter->drawPixmap(point, pixmap_, boundingRect());
//    painter->setBrush(QBrush(Qt::blue));
     painter->setPen(QPen(QColor(0, 160, 230),2));
     painter->drawRect(boundingRect());

//    if (option->state & QStyle::State_Selected) {
//        QPen p;
//        int wsize = 2;
//        p.setWidth(wsize);
//        p.setColor(QColor(0, 160, 230));
//        painter->setPen(p);
////        painter->drawRect(boundingRect());
//        painter->drawRect(1,1,pixmap_.width()+wsize,pixmap_.height()+wsize);
//    }

    Q_UNUSED(widget);
}

//void MoveItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
//{
//#ifdef MOUSE_MOVE_DEBUG
//    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
//#endif
//    this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
//}

void MoveItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(++zCounter_);

    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();

    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
//            setSelected(!isSelected());
        } else {
            QGraphicsItem::mousePressEvent(event);
        }
    }
    qDebug()<<"!!!!!!!!!!";
}

void MoveItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
//            setSelected(!isSelected());
        } else {
            QGraphicsItem::mousePressEvent(event);
        }
    }
}

void MoveItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    /*Scale 0.2 each turn of the wheel (which is usually 120.0 eights of degrees)*/
    qreal scaleFactor = 1.15;//1.0 + event->delta() * 0.2 / 120.0;
    if (event->delta()>0)
        setScale(scale() * scaleFactor);
    else
        setScale(scale() * (1/scaleFactor));
//    setTransformOriginPoint(mapFromScene(event->scenePos()));
}

//void MoveItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
//{
//  qInfo()<<"MoveItem::hoverEnterEvent";
//}

//void MoveItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
//{
//  qInfo()<<"MoveItem::hoverLeaveEvent";
//}

//void MoveItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
//{
//  qInfo()<<"MoveItem::hoverMoveEvent";
//}

void MoveItem::setRect(qreal x, qreal y, qreal w, qreal h)
{
    setRect(QRectF(x,y,w,h));
}

void MoveItem::setRect(const QRectF &rect)
{
    rect_ = rect;
//    qDebug()<<rect_;
    QGraphicsRectItem::setRect(rect);
}

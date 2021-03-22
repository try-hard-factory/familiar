#include "debug_macros.h"
#include "moveitem.h"
#include <QPen>

#include "Logger.h"

extern Logger logger;

#define MOUSE_MOVE_DEBUG

MovableCircle::MovableCircle(ECirclePos cp, double ar, QGraphicsItem *parent) :
    QGraphicsObject(parent), aspectRatio_(ar), circlePos_(cp)
{
//    aspectRatio_ = parent->boundingRect().height() / parent->boundingRect().width();
    setFlag(ItemClipsToShape, true);
    setCursor(QCursor(Qt::PointingHandCursor));
}

QRectF MovableCircle::boundingRect() const
{
    qreal adjust = 0.5;
    return QRectF(-5 - adjust, -5 - adjust,
                  10 + adjust, 10 + adjust);
}

QPainterPath MovableCircle::shape() const
{
    QPainterPath path;
    qreal adjust = 0.5;
    path.addEllipse(-5 - adjust, -5 - adjust,
                    10 + adjust, 10 + adjust);
    return path;
}

void MovableCircle::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setBrush(QBrush(QColor(0, 160, 230)));
    painter->setPen(QPen(QColor(0, 160, 230)));
    painter->drawEllipse(-5, -5, 10, 10);
}

void MovableCircle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    _shiftMouseCoords = this->pos() - mapToScene(event->pos());
    this->setCursor(QCursor(Qt::ClosedHandCursor));
}

void MovableCircle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto pos = mapToScene(event->pos() + _shiftMouseCoords);
//    pos.setX(qMin(pos.x(), pos.y()));
//    pos.setY(qMin(pos.x(), pos.y()));
    qreal xl = (pos.x() == 0) ? 1 : pos.x();
    qreal yl = (pos.y() == 0) ? 1 : pos.y();
    qreal arl = qAbs(xl / yl);

    if (circlePos_ == eBottomRight) {
        if (arl > aspectRatio_) {
            pos.setX(yl * aspectRatio_);
        } else {
            pos.setY(xl / aspectRatio_);

        }
    }

    if (circlePos_ == eTopLeft) {
        LOG_WARNING(logger, "Circle Pos: ", circlePos_, ", ", pos.x(), " ", pos.y());
        LOG_WARNING(logger, "Init Ar: ", aspectRatio_, ", Current Ar:", arl);
        if (arl > aspectRatio_) {
            LOG_DEBUG(logger, "> Before: ", pos.x(), ", ", pos.y(), " |", pos.y() * arl);
            pos.setY(xl / aspectRatio_);
            LOG_DEBUG(logger, "> After: ", pos.x(), ", ", pos.y(), " |", pos.y() * arl);
        } else {
            LOG_DEBUG(logger, "< Before: ", pos.x(), ", ", pos.y(), " |", pos.y() * arl);
            pos.setX(yl * aspectRatio_);
            LOG_DEBUG(logger, "< After: ", pos.x(), ", ", pos.y(), " |", pos.y() * arl);
        }
    }

    setPos(pos);
    emit circleMoved();
}

void MovableCircle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    this->setCursor(QCursor(Qt::PointingHandCursor));
}


/*********************************************************************/


MoveItem::MoveItem(uint64_t& zc, QGraphicsItem *parent) :
    QGraphicsItem(parent), zCounter_(zc)
{
    setZValue(zCounter_);
    qimage_ = QImage("kot.png");
//    auto qimage = QImage("kot.jpg");
//    qInfo() << qimage.width() << ' ' <<qimage.height();
    pixmap_ = QPixmap::fromImage(qimage_);
    _size = pixmap_.size();
    rect_ = qimage_.rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);

     double ar = _size.width() / _size.height();
     // Top Left
     _topLeftCircle = new MovableCircle(MovableCircle::eTopLeft, ar, this);
     _topLeftCircle->setPos(0, 0);
     // Top Right
     _topRightCircle = new MovableCircle(MovableCircle::eTopRight, ar, this);
     _topRightCircle->setPos(_size.width(), 0);
     // Bottom Right
     _bottomRightCircle = new MovableCircle(MovableCircle::eBottomRight, ar, this);
     _bottomRightCircle->setPos(_size.width(), _size.height());
     // Bottom Left
     _bottomLeftCircle = new MovableCircle(MovableCircle::eBottomLeft, ar, this);
     _bottomLeftCircle->setPos(0, _size.height());
     // Signals
     // If a delimiter point has been moved, so force the item to redraw

     connect(_topLeftCircle, &MovableCircle::circleMoved, this, [this](){
         _bottomLeftCircle->setPos( _topLeftCircle->pos().x(), _bottomLeftCircle->pos().y());
         _topRightCircle->setPos(_topRightCircle->pos().x(), _topLeftCircle->pos().y());
         update(); // force to Repaint
     });

     connect(_topRightCircle, &MovableCircle::circleMoved, this, [this](){
         _topLeftCircle->setPos(_topLeftCircle->pos().x(), _topRightCircle->pos().y());
         _bottomRightCircle->setPos(_topRightCircle->pos().x(), _bottomRightCircle->pos().y());
         update(); // force to Repaint
     });

     connect(_bottomLeftCircle, &MovableCircle::circleMoved, this, [this](){
         _topLeftCircle->setPos(_bottomLeftCircle->pos().x(), _topLeftCircle->pos().y());
         _bottomRightCircle->setPos(_bottomRightCircle->pos().x(), _bottomLeftCircle->pos().y());
         update(); // force to Repaint
     });

     connect(_bottomRightCircle, &MovableCircle::circleMoved, this, [this](){
         _bottomLeftCircle->setPos(_bottomLeftCircle->pos().x(), _bottomRightCircle->pos().y());
         _topRightCircle->setPos(_bottomRightCircle->pos().x(), _topRightCircle->pos().y());
         update(); // force to Repaint
     });
}

MoveItem::~MoveItem()
{

}


QRectF MoveItem::boundingRect() const
{
//    return QRectF (0,0,pixmap_.width(),pixmap_.height());
    qreal distX = sqrt(pow(_topLeftCircle->x() - _topRightCircle->x(),2) +
                       pow(_topLeftCircle->y() - _topRightCircle->y(),2)); // eucledian distance

    qreal distY = sqrt(pow(_topLeftCircle->x() - _bottomLeftCircle->x(),2) +
                       pow(_topLeftCircle->y() - _bottomLeftCircle->y(),2)); // eucledian distance


    return QRectF(qMin(_topLeftCircle->pos().x(), _topRightCircle->pos().x()) ,
                  qMin(_topLeftCircle->pos().y(), _bottomLeftCircle->pos().y()),
                  distX, distY);
}

void MoveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPointF point(0, 0);
    QRectF source(0, 0, pixmap_.width(),pixmap_.height());
     painter->drawImage(boundingRect(), qimage_);
//    painter->drawPixmap(point, pixmap_, boundingRect());
//    painter->setBrush(QBrush(Qt::blue));
     painter->setPen(QPen(QColor(0, 160, 230),2));
     painter->drawRect(rect_);

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

void MoveItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
#endif
    this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
}

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

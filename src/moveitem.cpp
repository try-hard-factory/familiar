#include "debug_macros.h"
#include "moveitem.h"
#include <QPen>


#include "Vec2d.h"
#include "Logger.h"

extern Logger logger;

#define MOUSE_MOVE_DEBUG

MovableCircle::MovableCircle(ECirclePos cp, QGraphicsItem *parent) :
    QGraphicsObject(parent), circlePos_(cp)
{
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
#define LOGPRINT(name) qDebug()<<#name<<"= "<<name;
void MovableCircle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto pos = mapToScene(event->pos() + _shiftMouseCoords);
    auto parent = parentItem();
    auto rect = parentItem()->boundingRect();

    QPointF a(rect.x(), rect.y());
    QPointF b(rect.x() + rect.width(), rect.y());
    QPointF c(rect.x() + rect.width(), rect.y() + rect.height());
    QPointF d(rect.x(), rect.y() + rect.height());


    if (circlePos_ == eTopRight) {
        Vec2d dc(c.x()-d.x(), c.y()-d.y());
        Vec2d cb(b.x()-c.x(), b.y()-c.y());
        Vec2d db(b.x()-d.x(), b.y()-d.y());
        Vec2d dk(pos.x()-d.x(), pos.y()-d.y());

        auto dc_len = dc.length();
        auto cb_len = cb.length();
        auto db_len = db.length();
        auto dk_len = dk.length();

        auto dkdb_dot = Vec2d<qreal>::dot(db, dk);
        auto cos_kdb = dkdb_dot/(db_len*dk_len);
        auto dd2_len = dk_len * cos_kdb;

        auto x =(dd2_len * dc_len) / (std::sqrt(cb_len * cb_len + dc_len * dc_len));
        auto y = std::sqrt(dd2_len * dd2_len - x * x);

        if (x < 10 || y < 10) return;
        pos.setX(d.x()+x);
        pos.setY(d.y()-y);
    }

    if (circlePos_ == eBottomRight) {
        Vec2d ad(d.x()-a.x(), d.y()-a.y());
        Vec2d dc(c.x()-d.x(), c.y()-d.y());
        Vec2d ac(c.x()-a.x(), c.y()-a.y());
        Vec2d ak(pos.x()-a.x(), pos.y()-a.y());

        auto ad_len = ad.length();
        auto dc_len = dc.length();
        auto ac_len = ac.length();
        auto ak_len = ak.length();

        auto akac_dot = Vec2d<qreal>::dot(ac, ak);
        auto cos_kac = akac_dot/(ac_len*ak_len);
        auto ad2_len = ak_len * cos_kac;

        auto x =(ad2_len * dc_len) / (std::sqrt(ad_len * ad_len + dc_len * dc_len));
        auto y = std::sqrt(ad2_len * ad2_len - x * x);

        if (x < 10 || y < 10) return;
        pos.setX(a.x()+x);
        pos.setY(a.y()+y);
    }

    if (circlePos_ == eTopLeft) {
        qDebug()<<this->parentItem()->boundingRect();
        Vec2d cb(b.x()-c.x(), b.y()-c.y());
        Vec2d ba(a.x()-b.x(), a.y()-b.y());
        Vec2d ca(a.x()-c.x(), a.y()-c.y());
        Vec2d ck(pos.x()-c.x(), pos.y()-c.y());

        auto cb_len = cb.length();
        auto ba_len = ba.length();
        auto ca_len = ca.length();
        auto ck_len = ck.length();

        auto ckca_dot = Vec2d<qreal>::dot(ca, ck);
        auto cos_kca = ckca_dot/(ca_len*ck_len);
        auto cd2_len = ck_len * cos_kca;

        auto y =(cd2_len * cb_len) / (std::sqrt(ba_len * ba_len + cb_len * cb_len));
        auto x = std::sqrt(cd2_len * cd2_len - y * y);

        if (x < 10 || y < 10) return;
        pos.setX(c.x()-x);
        pos.setY(c.y()-y);
    }

    if (circlePos_ == eBottomLeft) {
        qDebug()<<this->parentItem()->boundingRect();
        Vec2d ba(a.x()-b.x(), a.y()-b.y());
        Vec2d ad(d.x()-a.x(), d.y()-a.y());
        Vec2d bd(d.x()-b.x(), d.y()-b.y());
        Vec2d bk(pos.x()-b.x(), pos.y()-b.y());

        auto ba_len = ba.length();
        auto ad_len = ad.length();
        auto bd_len = bd.length();
        auto bk_len = bk.length();

        auto bkbd_dot = Vec2d<qreal>::dot(bd, bk);
        auto cos_kdb = bkbd_dot/(bd_len*bk_len);
        auto bd2_len = bk_len * cos_kdb;

        auto x =(bd2_len * ba_len) / (std::sqrt(ad_len * ad_len + ba_len * ba_len));
        auto y = std::sqrt(bd2_len * bd2_len - x * x);

        if (x < 10 || y < 10) return;
        pos.setX(b.x()-x);
        pos.setY(b.y()+y);
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


MoveItem::MoveItem(const QString& path, uint64_t& zc, QGraphicsItem *parent) :
    QGraphicsItem(parent), zCounter_(zc)
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

     // Top Left
     _topLeftCircle = new MovableCircle(MovableCircle::eTopLeft, this);
     _topLeftCircle->setPos(0, 0);
     auto parent0 = _topLeftCircle->parentItem();
     // Top Right
     _topRightCircle = new MovableCircle(MovableCircle::eTopRight, this);
     _topRightCircle->setPos(_size.width(), 0);
     // Bottom Right
     _bottomRightCircle = new MovableCircle(MovableCircle::eBottomRight, this);
     _bottomRightCircle->setPos(_size.width(), _size.height());
     // Bottom Left
     _bottomLeftCircle = new MovableCircle(MovableCircle::eBottomLeft, this);
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

MoveItem::MoveItem(const QImage& img, uint64_t& zc, QGraphicsItem *parent) :
    QGraphicsItem(parent), qimage_(img), zCounter_(zc)
{
    setZValue(zCounter_);
    pixmap_ = QPixmap::fromImage(qimage_);
    _size = pixmap_.size();
    rect_ = qimage_.rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);

     // Top Left
     _topLeftCircle = new MovableCircle(MovableCircle::eTopLeft, this);
     _topLeftCircle->setPos(0, 0);
     // Top Right
     _topRightCircle = new MovableCircle(MovableCircle::eTopRight, this);
     _topRightCircle->setPos(_size.width(), 0);
     // Bottom Right
     _bottomRightCircle = new MovableCircle(MovableCircle::eBottomRight, this);
     _bottomRightCircle->setPos(_size.width(), _size.height());
     // Bottom Left
     _bottomLeftCircle = new MovableCircle(MovableCircle::eBottomLeft, this);
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

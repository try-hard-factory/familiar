#include "itemgroup.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>

#include "Vec2d.h"
//#include "MovableCircle.h"
#include "Logger.h"

extern Logger logger;


#define MOUSE_MOVE_DEBUG

MovableCircle::MovableCircle(ECirclePos cp, QGraphicsItem *parent) :
    QGraphicsObject(parent), circlePos_(cp)
{
    qDebug()<<"PARENTADDR: "<<parent;
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
//    qDebug()<<"PARENTADDR: "<<parentItem();
    painter->setBrush(QBrush(QColor(0, 160, 230)));
    painter->setPen(QPen(QColor(0, 160, 230)));
    painter->drawEllipse(-5, -5, 10, 10);
}

void MovableCircle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    _shiftMouseCoords = this->pos() - mapToScene(event->pos());
    auto parent = parentItem();
    this->setCursor(QCursor(Qt::ClosedHandCursor));
}
#define LOGPRINT(name) qDebug()<<#name<<"= "<<name;
void MovableCircle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto pos = mapToScene(event->pos() + _shiftMouseCoords);

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

ItemGroup::ItemGroup(uint64_t& zc, QGraphicsItemGroup *parent) :
    QGraphicsItemGroup(parent),
    zCounter_(zc)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);

     // Top Left
     _topLeftCircle = new MovableCircle(MovableCircle::eTopLeft, this);
     _topLeftCircle->setPos(50, 50);
     _topLeftCircle->hide();
         auto parent0 = _topLeftCircle->parentItem();
     // Top Right
     _topRightCircle = new MovableCircle(MovableCircle::eTopRight, this);
     _topRightCircle->setPos(0, 0);
     _topRightCircle->hide();
     // Bottom Right
     _bottomRightCircle = new MovableCircle(MovableCircle::eBottomRight, this);
     _bottomRightCircle->setPos(0, 0);
     _bottomRightCircle->hide();
     // Bottom Left
     _bottomLeftCircle = new MovableCircle(MovableCircle::eBottomLeft, this);
     _bottomLeftCircle->setPos(0, 0);
     _bottomLeftCircle->hide();

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

void ItemGroup::addItem(QGraphicsItem* item)
{
    addToGroup(item);
    auto br = sceneBoundingRect();
    qDebug()<<br;
    _topLeftCircle->setPos(br.x(), br.y());
    _topRightCircle->setPos(br.x()+br.width(), br.y());
    _bottomRightCircle->setPos(br.x()+br.width(), br.y()+br.height());
    _bottomLeftCircle->setPos(br.x(), br.y()+br.height());
    MovableCirclesSetVisible(true);
}

void ItemGroup::printChilds()
{
    auto childs = childItems();
    for (auto& it : childs) {
        LOG_DEBUG(logger, "CHILDREN: ", it);
    }
}

//QRectF ItemGroup::boundingRect() const
//{
////    return QRectF (0,0,pixmap_.width(),pixmap_.height());
//    qreal distX = sqrt(pow(_topLeftCircle->x() - _topRightCircle->x(),2) +
//                       pow(_topLeftCircle->y() - _topRightCircle->y(),2)); // eucledian distance

//    qreal distY = sqrt(pow(_topLeftCircle->x() - _bottomLeftCircle->x(),2) +
//                       pow(_topLeftCircle->y() - _bottomLeftCircle->y(),2)); // eucledian distance


//    return QRectF(qMin(_topLeftCircle->pos().x(), _topRightCircle->pos().x()) ,
//                  qMin(_topLeftCircle->pos().y(), _bottomLeftCircle->pos().y()),
//                  distX, distY);
//}

void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
#endif
    auto br = sceneBoundingRect();
    _topLeftCircle->setPos(br.x(), br.y());
    _topRightCircle->setPos(br.x()+br.width(), br.y());
    _bottomRightCircle->setPos(br.x()+br.width(), br.y()+br.height());
    _bottomLeftCircle->setPos(br.x(), br.y()+br.height());
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
    QGraphicsItem::hoverLeaveEvent( event );
}

void ItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
//    QPointF pt = event->pos();              // The current position of the mouse
//    qreal drx = pt.x() - boundingRect().right();    // Distance between the mouse and the right
//    qreal dlx = pt.x() - boundingRect().left();     // Distance between the mouse and the left

//    qreal dby = pt.y() - boundingRect().top();      // Distance between the mouse and the top
//    qreal dty = pt.y() - boundingRect().bottom();   // Distance between the mouse and the bottom

////    LOG_WARNING(logger, drx, " ", dlx, " ", dby, " ", dty);
//    // If the mouse position is within a radius of 7
//    // to a certain side( top, left, bottom or right)
//    // we set the Flag in the Corner Flags Register

//    cornerFlags_ = 0;
//    if( dby < 10 && dby > -10 ) cornerFlags_ |= Top;       // Top side
//    if( dty < 10 && dty > -10 ) cornerFlags_ |= Bottom;    // Bottom side
//    if( drx < 10 && drx > -10 ) cornerFlags_ |= Right;     // Right side
//    if( dlx < 10 && dlx > -10 ) cornerFlags_ |= Left;      // Left side

//    switch (cornerFlags_) {
//    case TopLeft:
//    case TopRight:
//    case BottomLeft:
//    case BottomRight: {
//        setCursor(Qt::BusyCursor);
//        break;
//    }
//    default:
//        setCursor(Qt::CrossCursor);
//        break;
//    }
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
    MovableCirclesSetVisible(false);
    auto childs = childItems();
    for (auto& it : childs) {        
        if (it->type() != eBorderDot) {
            removeFromGroup(it);
            LOG_DEBUG(logger, "REMOVE ", it);
        }
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


void ItemGroup::MovableCirclesSetVisible(bool visibility)
{
    _topLeftCircle->setVisible(visibility);
    _topRightCircle->setVisible(visibility);
    _bottomRightCircle->setVisible(visibility);
    _bottomLeftCircle->setVisible(visibility);
}

#include "itemgroup.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>

#include "Vec2d.h"
#include "Logger.h"

extern Logger logger;


#define MOUSE_MOVE_DEBUG


/*********************************************************************/
#include <QPainter>
#include <QDebug>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <math.h>
#include "borderdot.h"
#include "moveitem.h"

static const double Pi = 3.14159265358979323846264338327950288419717;
static double TwoPi = 2.0 * Pi;

static qreal normalizeAngle(qreal angle)
{
    while (angle < 0)
        angle += TwoPi;
    while (angle > TwoPi)
        angle -= TwoPi;
    return angle;
}


ItemGroup::~ItemGroup()
{
    for(int i = 0; i < 4; i++){
        delete cornerGrabber[i];
    }
}

QPointF ItemGroup::previousPosition() const
{
    return m_previousPosition;
}


void ItemGroup::setPreviousPosition(const QPointF previousPosition)
{
    if (m_previousPosition == previousPosition)
        return;

    m_previousPosition = previousPosition;
    emit previousPositionChanged();
}


ItemGroup::ItemGroup(uint64_t& zc, QGraphicsItemGroup *parent) :
    QGraphicsItemGroup(parent),
    zCounter_(zc),
    m_cornerFlags(0),
    m_actionFlags(ResizeState)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable|ItemSendsGeometryChanges);
//    setFlags(ItemStacksBehindParent);
}

void ItemGroup::addItemToGroup(QGraphicsItem* item)
{
    if (item == this || item->type() == eBorderDot) return;
    LOG_DEBUG(logger, "ItemGroup::addItem: ", item);
    addToGroup(item);

    if (item->type() != eBorderDot) {
        items_.emplace_back(item);
    }

    auto childs = childItems();
    auto scene_tmp = childs.first()->sceneBoundingRect();
    for (auto& it : childs) {
        if (it->type() == eBorderDot) continue;
        scene_tmp = scene_tmp.united(it->sceneBoundingRect());
        auto widget = qgraphicsitem_cast<MoveItem*>(it);
        widget->setInGroup(true);
        qDebug()<<"CHILD: "<< mapToScene(widget->boundingRect()).boundingRect();
    }
    sceneRectItemGroup_ = scene_tmp;

    qDebug()<<"BR: "<< rectItemGroup_;
    qDebug()<<"SBR: "<< sceneRectItemGroup_;
    auto tmpRect = sceneRectItemGroup_;
    tmpRect.translate( - sceneRectItemGroup_.topLeft().x() , - sceneRectItemGroup_.topLeft().y());
    rectItemGroup_ = tmpRect;
    qDebug()<<"tmpRect: "<<tmpRect;
    qDebug()<<"SBR mod1: "<< mapRectFromParent(rectItemGroup_);
    qDebug()<<"SBR mod2: "<< mapRectFromScene(rectItemGroup_);
    qDebug()<<"SBR mod3: "<< mapRectToParent(rectItemGroup_);
    qDebug()<<"SBR mod4: "<< mapRectToScene(rectItemGroup_);


    if (!cornerGrabber[0]) {
        for(int i = 0; i < 4; i++){
            cornerGrabber[i] = new DotSignal(this);
        }
        hideGrabbers();
    }
    printDotsCoords("addItemToGroup 0");
    setPositionGrabbers();
    setVisibilityGrabbers();
    printDotsCoords("addItemToGroup 1");
}

void ItemGroup::removeItemFromGroup(QGraphicsItem* item)
{
    if (item->type() != eBorderDot) {
        auto widget = qgraphicsitem_cast<MoveItem*>(item);
        widget->setInGroup(false);
    }
    items_.erase(std::remove_if(items_.begin(), items_.end(), [&](QGraphicsItem* i) { return i == item; }),
                  items_.end());

    removeFromGroup(item);
    //need remove from rects
    //check that empty and delete dots
}


void ItemGroup::printChilds()
{
    auto childs = childItems();
    for (auto& it : childs) {
        LOG_DEBUG(logger, "CHILDREN: ", it);
    }
}

QRectF ItemGroup::currentSceneBoundingRect() const {
//    return boundingRect();
    return sceneRectItemGroup_;
}
QRectF ItemGroup::boundingRect() const
{
//    qDebug()<<this->pos();
//    return QGraphicsItemGroup::boundingRect();
//    return rectItemGroup_;
    return sceneRectItemGroup_;
}

void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pt = event->pos();
    if(m_actionFlags == ResizeState){
        switch (m_cornerFlags) {
        case TopLeft:
            resizeTopLeft(pt);
            break;
        case TopRight:
            resizeTopRight(pt);
            break;
        case BottomLeft:
            resizeBottomLeft(pt);
            break;
        case BottomRight:
            resizeBottomRight(pt);
            break;
        default:
            if (m_leftMouseButtonPressed) {
//                setCursor(Qt::ClosedHandCursor);
                qDebug()<<'\n';
                qDebug()<< "MOVE: "<< mapToScene(event->pos()+ shiftMouseCoords_);
                qDebug()<<"ItemGroup:: pos: "<<event->pos()<<", scenePos: "<<event->scenePos();
                qDebug()<<"ItemGroup:: itemGroup_ pos: "<<pos()<<", scenePos: "<<scenePos();
                qDebug()<<'\n';
//                this->setPos(mapToScene(event->pos()+ shiftMouseCoords_));
                auto dx = event->scenePos().x() - m_previousPosition.x();
                auto dy = event->scenePos().y() - m_previousPosition.y();
                moveBy(dx,dy);
                setPreviousPosition(event->scenePos());
                emit signalMove(this, dx, dy);
            }
            break;
        }
    } else {
        switch (m_cornerFlags) {
        case TopLeft:
        case TopRight:
        case BottomLeft:
        case BottomRight: {
            rotateItem(pt);
            break;
        }
        default:
            if (m_leftMouseButtonPressed) {
                  LOG_DEBUG(logger, "MOVE: ");
                setCursor(Qt::ClosedHandCursor);
                auto dx = event->scenePos().x() - m_previousPosition.x();
                auto dy = event->scenePos().y() - m_previousPosition.y();
                moveBy(dx,dy);
                setPreviousPosition(event->scenePos());
                emit signalMove(this, dx, dy);
            }
            break;
        }
    }
    QGraphicsItemGroup::mouseMoveEvent(event);
}

void ItemGroup::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(++zCounter_);
    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();
    qDebug()<<"shiftMouseCoords_: "<<shiftMouseCoords_;
    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = true;
        setPreviousPosition(event->scenePos());
        emit clicked(this);
    }
    QGraphicsItemGroup::mousePressEvent(event);
    LOG_DEBUG(logger, "EventPos: (", event->pos().x(),";",event->pos().y(), "), Pos: (", pos().x(),";",pos().y(),")");
}

void ItemGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = false;
    }
    QGraphicsItemGroup::mouseReleaseEvent(event);
}

void ItemGroup::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    m_actionFlags = (m_actionFlags == ResizeState)?RotationState:ResizeState;
    setVisibilityGrabbers();
    QGraphicsItemGroup::mouseDoubleClickEvent(event);
}

void ItemGroup::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsItem::hoverEnterEvent(event);
}

void ItemGroup::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_cornerFlags = 0;
    QGraphicsItem::hoverLeaveEvent( event );
}

void ItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - currentSceneBoundingRect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - currentSceneBoundingRect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - currentSceneBoundingRect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - currentSceneBoundingRect().bottom();   // Distance between the mouse and the bottom


    m_cornerFlags = 0;
    if( dby < 10 && dby > -10 ) m_cornerFlags |= Top;       // Top side
    if( dty < 10 && dty > -10 ) m_cornerFlags |= Bottom;    // Bottom side
    if( drx < 10 && drx > -10 ) m_cornerFlags |= Right;     // Right side
    if( dlx < 10 && dlx > -10 ) m_cornerFlags |= Left;      // Left side

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


QVariant ItemGroup::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value)
{
    switch (change) {
    case QGraphicsItemGroup::ItemSelectedChange:
        m_actionFlags = ResizeState;
        break;
    default:
        break;
    }
    return QGraphicsItemGroup::itemChange(change, value);
}

ItemGroup::new_size_t ItemGroup::calculateNewSize(const QRectF& tmpRect, MoveItem* widget)
{
    qreal old_ig_w = sceneRectItemGroup_.width();
    qreal old_ig_h = sceneRectItemGroup_.height();
    qreal new_ig_w = tmpRect.width();
    qreal new_ig_h = tmpRect.height();

    qreal old_h = abs(widget->sceneBoundingRect().bottom() -  widget->sceneBoundingRect().top());
    qreal new_h = (old_h * new_ig_h) / old_ig_h;
    qreal old_w = abs(widget->sceneBoundingRect().left() - widget->sceneBoundingRect().right());
    qreal new_w = (old_w * new_h) / old_h;

    qreal new_x = 0;
    qreal new_y = 0;

    if (widget->sceneBoundingRect().left() == currentSceneBoundingRect().left()) {
        qreal aa = abs(widget->sceneBoundingRect().top() - sceneRectItemGroup_.bottom());
        qreal yy = (aa*new_ig_w)/old_ig_w;
        qreal y_delta = abs(new_ig_h - yy);

        new_x = tmpRect.topLeft().x();
        new_y = tmpRect.topLeft().y() + y_delta;
    } else if (widget->sceneBoundingRect().top() == currentSceneBoundingRect().top()) {
        qreal aa = abs(widget->sceneBoundingRect().left() - sceneRectItemGroup_.right());
        qreal xx = (aa*new_ig_h)/old_ig_h;
        qreal x_delta = abs(new_ig_w - xx);

        new_x = tmpRect.topLeft().x() + x_delta;
        new_y = tmpRect.topLeft().y();
    } else  {
        qreal qq = abs(sceneRectItemGroup_.left() - widget->sceneBoundingRect().left());
        qreal ww = abs(sceneRectItemGroup_.top() - widget->sceneBoundingRect().top());
        qreal new_x_delta = (new_ig_w * qq) / old_ig_w;
        qreal new_y_delta = (new_ig_h * ww) / old_ig_h;

        new_x = tmpRect.left() + new_x_delta;
        new_y = tmpRect.top() + new_y_delta;
    }

    return {new_x, new_y, new_w, new_h};
}

void ItemGroup::resizeTopLeft(const QPointF &pt)
{
    auto pos = pt;
    auto rect = currentSceneBoundingRect();

    QPointF a(rect.x(), rect.y());
    QPointF b(rect.x() + rect.width(), rect.y());
    QPointF c(rect.x() + rect.width(), rect.y() + rect.height());
    QPointF d(rect.x(), rect.y() + rect.height());

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


    QRectF tmpRect = currentSceneBoundingRect();

    qreal heightOffset = ( pos.y() - rect.bottom() );
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );

    qreal widthOffset = ( pos.x() - rect.right() );
    if( widthOffset < 0)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );

    tmpRect.translate( currentSceneBoundingRect().width() - tmpRect.width() , currentSceneBoundingRect().height() - tmpRect.height() );
    prepareGeometryChange();


    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        auto new_size = calculateNewSize(tmpRect, widget);

        widget->setX(new_size.new_x);
        widget->setY(new_size.new_y);
        widget->setRect(new_size.new_x, new_size.new_y, new_size.new_w, new_size.new_h);
    }

    sceneRectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeTopRight(const QPointF &pt)
{
    auto pos = pt;
    auto rect = currentSceneBoundingRect();

    QPointF a(rect.x(), rect.y());
    QPointF b(rect.x() + rect.width(), rect.y());
    QPointF c(rect.x() + rect.width(), rect.y() + rect.height());
    QPointF d(rect.x(), rect.y() + rect.height());

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

    QRectF tmpRect = currentSceneBoundingRect();

    qreal heightOffset = ( pos.y() - rect.bottom() );
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );

    qreal widthOffset = ( pos.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 0)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );

    tmpRect.translate( 0 , currentSceneBoundingRect().height() - tmpRect.height() );

    prepareGeometryChange();


    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        auto new_size = calculateNewSize(tmpRect, widget);

        widget->setX(new_size.new_x);
        widget->setY(new_size.new_y);
        widget->setRect(new_size.new_x, new_size.new_y, new_size.new_w, new_size.new_h);
    }

    sceneRectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeBottomLeft(const QPointF &pt)
{
    auto pos = pt;
    auto rect = currentSceneBoundingRect();

    QPointF a(rect.x(), rect.y());
    QPointF b(rect.x() + rect.width(), rect.y());
    QPointF c(rect.x() + rect.width(), rect.y() + rect.height());
    QPointF d(rect.x(), rect.y() + rect.height());

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


    QRectF tmpRect = currentSceneBoundingRect();

    qreal heightOffset =  ( pos.y() - tmpRect.top() );
    if( heightOffset < 11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );

    qreal widthOffset = ( pos.x() - rect.right() );
    if( widthOffset < 0)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );


    tmpRect.translate( currentSceneBoundingRect().width() - tmpRect.width() , 0 );
    prepareGeometryChange();


    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        auto new_size = calculateNewSize(tmpRect, widget);

        widget->setX(new_size.new_x);
        widget->setY(new_size.new_y);
        widget->setRect(new_size.new_x, new_size.new_y, new_size.new_w, new_size.new_h);
    }

    sceneRectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeBottomRight(const QPointF &pt)
{
    auto pos = pt;
    auto rect = currentSceneBoundingRect();

    QPointF a(rect.x(), rect.y());
    QPointF b(rect.x() + rect.width(), rect.y());
    QPointF c(rect.x() + rect.width(), rect.y() + rect.height());
    QPointF d(rect.x(), rect.y() + rect.height());

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


    QRectF tmpRect = currentSceneBoundingRect();
    if( pos.y() < tmpRect.top() )
        return;
    qreal heightOffset =  ( pos.y() - tmpRect.top() );
    if( heightOffset < 11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    if( pos.x() < tmpRect.left() )
        return;
    qreal widthOffset =  ( pos.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 0)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );

    prepareGeometryChange();


    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        auto new_size = calculateNewSize(tmpRect, widget);

        widget->setX(new_size.new_x);
        widget->setY(new_size.new_y);
        widget->setRect(new_size.new_x, new_size.new_y, new_size.new_w, new_size.new_h);
    }

    sceneRectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::rotateItem(const QPointF &pt)
{
//    QRectF tmpRect = currentSceneBoundingRect();
//    QPointF center = currentSceneBoundingRect().center();
//    QPointF corner;
//    switch (m_cornerFlags) {
//    case TopLeft:
//        corner = tmpRect.topLeft();
//        break;
//    case TopRight:
//        corner = tmpRect.topRight();
//        break;
//    case BottomLeft:
//        corner = tmpRect.bottomLeft();
//        break;
//    case BottomRight:
//        corner = tmpRect.bottomRight();
//        break;
//    default:
//        break;
//    }

//    QLineF lineToTarget(center,corner);
//    QLineF lineToCursor(center, pt);
//    // Angle to Cursor and Corner Target points
//    qreal angleToTarget = ::acos(lineToTarget.dx() / lineToTarget.length());
//    qreal angleToCursor = ::acos(lineToCursor.dx() / lineToCursor.length());

//    if (lineToTarget.dy() < 0)
//        angleToTarget = TwoPi - angleToTarget;
//    angleToTarget = normalizeAngle((Pi - angleToTarget) + Pi / 2);

//    if (lineToCursor.dy() < 0)
//        angleToCursor = TwoPi - angleToCursor;
//    angleToCursor = normalizeAngle((Pi - angleToCursor) + Pi / 2);

//    // Result difference angle between Corner Target point and Cursor Point
//    auto resultAngle = angleToTarget - angleToCursor;

//    QTransform trans = transform();
//    trans.translate( center.x(), center.y());
//    trans.rotateRadians(rotation() + resultAngle, Qt::ZAxis);
//    trans.translate( -center.x(),  -center.y());
//    setTransform(trans);
}

void ItemGroup::setPositionGrabbers()
{
    if (!cornerGrabber[0]) return;

    QRectF tmpRect = currentSceneBoundingRect();

    cornerGrabber[GrabberTopLeft]->setPos(tmpRect.topLeft().x(), tmpRect.topLeft().y());
    cornerGrabber[GrabberTopRight]->setPos(tmpRect.topRight().x(), tmpRect.topRight().y());
    cornerGrabber[GrabberBottomLeft]->setPos(tmpRect.bottomLeft().x(), tmpRect.bottomLeft().y());
    cornerGrabber[GrabberBottomRight]->setPos(tmpRect.bottomRight().x(), tmpRect.bottomRight().y());
}

void ItemGroup::setVisibilityGrabbers()
{
    if (!cornerGrabber[0]) return;
    cornerGrabber[GrabberTopLeft]->setVisible(true);
    cornerGrabber[GrabberTopRight]->setVisible(true);
    cornerGrabber[GrabberBottomLeft]->setVisible(true);
    cornerGrabber[GrabberBottomRight]->setVisible(true);
    for(int i = 0; i < 4; i++){
        cornerGrabber[i]->setEnabled(true);
    }
}

void ItemGroup::hideGrabbers()
{
    if (!cornerGrabber[0]) return;
    for(int i = 0; i < 4; i++){
        cornerGrabber[i]->setVisible(false);
        cornerGrabber[i]->setEnabled(false);
    }
}


void ItemGroup::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen( QPen(Qt::green, 3) );
    painter->drawEllipse(this->pos(), 6,6);
    painter->setPen( QPen(Qt::black, 1) );
    painter->drawLine(-99999, this->pos().y(), 9999, this->pos().y());
    painter->drawLine(this->pos().x(), -99999, this->pos().x(), 99999);

    painter->setPen( QPen(Qt::darkMagenta, 3) );
    painter->drawEllipse(this->scenePos(), 3,3);
    painter->setPen( QPen(Qt::darkRed, 1) );
    painter->drawLine(-99999, this->scenePos().y(), 9999, this->scenePos().y());
    painter->drawLine(this->scenePos().x(), -99999, this->scenePos().x(), 99999);
    if (isEmpty()) return;
//    qInfo()<<"ItemGroup::paint "<< items_.size();
//    qInfo()<<"ItemGroup::boundingRect "<< this->boundingRect();

//    painter->save();

////    auto tl = this->realRect().topLeft();
////    auto br = this->realRect().bottomRight();
//    auto tl = this->boundingRect().topLeft();
//    auto br = this->boundingRect().bottomRight();
//    int wsize = 5;
//    br.rx() += wsize;
//    br.ry() += wsize;

//    painter->setPen( QPen(Qt::green, wsize) );

//    QRectF r(tl, br);
//    painter->drawRect(r);
//    painter->restore();
}

void ItemGroup::printDotsCoords(const std::string& text) const
{
    LOG_WARNING(logger, "! ", text);
    for(int i = 0; i < 4; i++){
        qDebug()<< cornerGrabber[i]->pos();
    }
    LOG_WARNING(logger, "! ", text);
}

void ItemGroup::clearItemGroup()
{
    hideGrabbers();
    auto childs = childItems();
    for (auto& it : childs) {        
        removeItemFromGroup(it);
        LOG_DEBUG(logger, "REMOVE ", it);
    }

    for(int i = 0; i < 4; i++){
        delete cornerGrabber[i];
        cornerGrabber[i] = nullptr;
    }
    sceneRectItemGroup_ = QRectF();
}

bool ItemGroup::isContain(const QGraphicsItem *item) const
{
    if (this == item->parentItem()) return true;

    return false;
}

bool ItemGroup::isThisDots(const QGraphicsItem *item) const
{
    if (item->type() == eBorderDot) return true;
    return false;
}

bool ItemGroup::isEmpty() const
{
    return items_.empty();
}

void ItemGroup::incZ()
{
    setZValue(++zCounter_);
}

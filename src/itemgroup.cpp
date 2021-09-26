#include "itemgroup.h"
#include <QGuiApplication>
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
    setFiltersChildEvents(true);
    setFlags(ItemIsSelectable|ItemSendsGeometryChanges);
}


QRectF ItemGroup::calcNewBr()
{
    if (items_.empty()) return QRectF();

    qreal min_x = items_.first()->pos().x();
    qreal min_y = items_.first()->pos().y();
    qreal max_x = items_.first()->pos().x() + items_.first()->boundingRect().width();
    qreal max_y = items_.first()->pos().y() + items_.first()->boundingRect().height();

    for (auto& item : items_) {
        qreal cur_min_x = item->pos().x();
        qreal cur_min_y = item->pos().y();
        qreal cur_max_x = cur_min_x + item->boundingRect().width();
        qreal cur_max_y = cur_min_y + item->boundingRect().height();

        if (cur_min_x < min_x) min_x = cur_min_x;
        if (cur_min_y < min_y) min_y = cur_min_y;
        if (max_x < cur_max_x) max_x = cur_max_x;
        if (max_y < cur_max_y) max_y = cur_max_y;
    }

    return QRectF(min_x, min_y, max_x - min_x, max_y - min_y);
}


void ItemGroup::addItemToGroup(QGraphicsItem* item)
{
    if (item == this || item->type() == eBorderDot) return;
//    qDebug()<<"Item pointer : "<< (void *)item;
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
    }

    sceneRectItemGroup_ = scene_tmp;
    rectItemGroup_ = calcNewBr();

    if (!cornerGrabber[0]) {
        for(int i = 0; i < 4; i++){
            cornerGrabber[i] = new DotSignal(this);
//            qDebug()<<"ALLOCATE: "<< (void*)cornerGrabber[i];
        }
        hideGrabbers();
    }

    setPositionGrabbers();
    updateScaleControl();
    setVisibilityGrabbers();
}


void ItemGroup::removeItemFromGroup(QGraphicsItem* item)
{
    if (item->type() != eBorderDot) {
        auto widget = qgraphicsitem_cast<MoveItem*>(item);
        widget->setInGroup(false);
//        LOG_DEBUG(logger, "REMOVE ", item, ", type = ", item->type());
    }
    items_.erase(std::remove_if(items_.begin(), items_.end(), [&](QGraphicsItem* i) { return i == item; }),
                  items_.end());

    removeFromGroup(item);
    rectItemGroup_ = calcNewBr();
    setPositionGrabbers();
    //need remove from rects
    //check that empty and delete dots
}


void ItemGroup::printChilds()
{
    auto childs = childItems();
    for (auto& it : childs) {
        qDebug()<<it->type()<<' '<<it->pos();
        //LOG_DEBUG(logger, "CHILDREN: ", it), ;
    }
}


QRectF ItemGroup::currentSceneBoundingRect() const {
    return boundingRect();
}


QRectF ItemGroup::boundingRect() const
{
    return rectItemGroup_;
}


void ItemGroup::notifyCursorUpdater(QGraphicsSceneMouseEvent *event, qreal sf)
{
    if ( (event->buttons() & Qt::LeftButton) == 0) {
        if (!cornerGrabber[0]) return;
        QPointF pt = event->scenePos();

        auto tlPoint = pt - cornerGrabber[0]->scenePos();
        auto trPoint = pt - cornerGrabber[1]->scenePos();
        auto blPoint = pt - cornerGrabber[2]->scenePos();
        auto brPoint = pt - cornerGrabber[3]->scenePos();

        auto tlLen = std::sqrt(std::pow(tlPoint.x(), 2) + std::pow(tlPoint.y(), 2));
        auto trLen = std::sqrt(std::pow(trPoint.x(), 2) + std::pow(trPoint.y(), 2));
        auto blLen = std::sqrt(std::pow(blPoint.x(), 2) + std::pow(blPoint.y(), 2));
        auto brLen = std::sqrt(std::pow(brPoint.x(), 2) + std::pow(brPoint.y(), 2));

        m_cornerFlags = 0;
        int x = 4;
        if ( (tlLen * sf - x) < 0 ) m_cornerFlags = (Top|Left);
        if (trLen * sf < x) m_cornerFlags = (Top|Right);
        if (blLen * sf < x) m_cornerFlags = (Bottom|Left);
        if (brLen * sf < x) m_cornerFlags = (Bottom|Right);
    }


    switch (m_cornerFlags) {
    case TopLeft:
    case BottomRight:  {
        if (sem_ == 0) {
            QGuiApplication::setOverrideCursor(QCursor(Qt::SizeFDiagCursor));
            sem_ = 1;
        }

    } break;
    case TopRight:
    case BottomLeft: {
        if (sem_ == 0) {
            QGuiApplication::setOverrideCursor(QCursor(Qt::SizeBDiagCursor));
            sem_ = 2;
        }
    } break;
    default: {
        sem_ = 0;
        QGuiApplication::restoreOverrideCursor();
    } break;
    }
}

void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{

    QPointF pt = event->pos();              // The current position of the mouse
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
                auto dx = event->scenePos().x() - m_previousPosition.x();
                auto dy = event->scenePos().y() - m_previousPosition.y();
                moveBy(dx,dy);
                setPreviousPosition(event->scenePos());
//                emit signalMove(this, dx, dy);
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
                setCursor(Qt::ClosedHandCursor);
                auto dx = event->scenePos().x() - m_previousPosition.x();
                auto dy = event->scenePos().y() - m_previousPosition.y();
                moveBy(dx,dy);
                setPreviousPosition(event->scenePos());
//                emit signalMove(this, dx, dy);
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

    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = true;
        setPreviousPosition(event->scenePos());
        emit clicked(this);
    }

    QGraphicsItemGroup::mousePressEvent(event);
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
    Q_UNUSED(event)
//    QGraphicsItemGroup::hoverEnterEvent(event);
}


void ItemGroup::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
//    m_cornerFlags = 0;
//    QGraphicsItemGroup::hoverLeaveEvent( event );
}


//void ItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
//{
//    QPointF pt = event->pos();              // The current position of the mouse
//    qreal drx = pt.x() - boundingRect().right();    // Distance between the mouse and the right
//    qreal dlx = pt.x() - boundingRect().left();     // Distance between the mouse and the left

//    qreal dby = pt.y() - boundingRect().top();      // Distance between the mouse and the top
//    qreal dty = pt.y() - boundingRect().bottom();   // Distance between the mouse and the bottom



//    m_cornerFlags = 0;
//    if( dby < 10 && dby > -10 ) m_cornerFlags |= Top;       // Top side
//    if( dty < 10 && dty > -10 ) m_cornerFlags |= Bottom;    // Bottom side
//    if( drx < 10 && drx > -10 ) m_cornerFlags |= Right;     // Right side
//    if( dlx < 10 && dlx > -10 ) m_cornerFlags |= Left;      // Left side

//    qInfo()<<"ItemGroup::hoverMoveEvent, DTY = "<< dty <<", DRX = " << drx << ", FLAG = " << m_cornerFlags;

//    switch (m_cornerFlags) {
//    case TopLeft:
//    case BottomRight:  {
//        setCursor(Qt::SizeFDiagCursor);
//    } break;
//    case TopRight:
//    case BottomLeft: {
//        setCursor(Qt::SizeBDiagCursor);
//    } break;
//    default:
//    setCursor(Qt::ClosedHandCursor);
//    break;
//    }
//}


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
    qreal old_ig_w = boundingRect().width();
    qreal old_ig_h = boundingRect().height();
    qreal new_ig_w = tmpRect.width();
    qreal new_ig_h = tmpRect.height();

    qreal old_h = abs(widget->boundingRect().bottom() -  widget->boundingRect().top());
    qreal new_h = (old_h * new_ig_h) / old_ig_h;
    qreal old_w = abs(widget->boundingRect().left() - widget->boundingRect().right());
    qreal new_w = (old_w * new_h) / old_h;

    qreal new_x = 0;
    qreal new_y = 0;

    if (widget->pos().x() == boundingRect().left()) {
        qreal aa = abs(widget->pos().y() - boundingRect().bottom());
        qreal yy = (aa*new_ig_w)/old_ig_w;
        qreal y_delta = abs(new_ig_h - yy);

        new_x = tmpRect.topLeft().x();
        new_y = tmpRect.topLeft().y() + y_delta;
    } else if (widget->pos().y() == boundingRect().top()) {
        qreal aa = abs(widget->pos().x() - boundingRect().right());
        qreal xx = (aa*new_ig_h)/old_ig_h;
        qreal x_delta = abs(new_ig_w - xx);

        new_x = tmpRect.topLeft().x() + x_delta;
        new_y = tmpRect.topLeft().y();
    } else  {
        qreal qq = abs(boundingRect().left() - widget->pos().x());
        qreal ww = abs(boundingRect().top() - widget->pos().y());
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

    if (x < 10 || y < 10) {
        return;
    }

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

        widget->setPos(new_size.new_x, new_size.new_y);
        widget->setRect(new_size.new_x, new_size.new_y, new_size.new_w, new_size.new_h);
    }

    rectItemGroup_ = tmpRect;
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

    rectItemGroup_ = tmpRect;
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

    rectItemGroup_ = tmpRect;
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

    rectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}


void ItemGroup::rotateItem(const QPointF &pt)
{
    QRectF tmpRect = currentSceneBoundingRect();
    QPointF center = currentSceneBoundingRect().center();
    QPointF corner;
    switch (m_cornerFlags) {
    case TopLeft:
        corner = tmpRect.topLeft();
        break;
    case TopRight:
        corner = tmpRect.topRight();
        break;
    case BottomLeft:
        corner = tmpRect.bottomLeft();
        break;
    case BottomRight:
        corner = tmpRect.bottomRight();
        break;
    default:
        break;
    }

    QLineF lineToTarget(center,corner);
    QLineF lineToCursor(center, pt);
    // Angle to Cursor and Corner Target points
    qreal angleToTarget = ::acos(lineToTarget.dx() / lineToTarget.length());
    qreal angleToCursor = ::acos(lineToCursor.dx() / lineToCursor.length());

    if (lineToTarget.dy() < 0)
        angleToTarget = TwoPi - angleToTarget;
    angleToTarget = normalizeAngle((Pi - angleToTarget) + Pi / 2);

    if (lineToCursor.dy() < 0)
        angleToCursor = TwoPi - angleToCursor;
    angleToCursor = normalizeAngle((Pi - angleToCursor) + Pi / 2);

    // Result difference angle between Corner Target point and Cursor Point
    auto resultAngle = angleToTarget - angleToCursor;

    QTransform trans = transform();
    trans.translate( center.x(), center.y());
    trans.rotateRadians(rotation() + resultAngle, Qt::ZAxis);
    trans.translate( -center.x(),  -center.y());
    setTransform(trans);
}


void ItemGroup::setPositionGrabbers()
{
    if (!cornerGrabber[0]) return;

    QRectF tmpRect = boundingRect();

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
#ifdef GRID_DEBUG
    painter->save();
    painter->setPen( QPen(Qt::green, 3) );
    painter->drawEllipse(this->pos(), 6,6);
    painter->drawEllipse(this->x(), this->y(), 6,6);
    painter->setPen( QPen(Qt::black, 1) );
    painter->drawLine(-99999, this->pos().y(), 9999, this->pos().y());
    painter->drawLine(this->pos().x(), -99999, this->pos().x(), 99999);


    painter->setPen( QPen(Qt::darkMagenta, 3) );
    painter->drawEllipse(this->scenePos(), 3,3);
    painter->setPen( QPen(Qt::darkRed, 1) );
    painter->drawLine(-99999, this->scenePos().y(), 9999, this->scenePos().y());
    painter->drawLine(this->scenePos().x(), -99999, this->scenePos().x(), 99999);
    painter->restore();
#endif
    if (isEmpty()) return;
}


void ItemGroup::printDotsCoords(const std::string& text) const
{
    for(int i = 0; i < 4; i++){
        qDebug()<< cornerGrabber[i]->pos();
    }
}


void ItemGroup::clearItemGroup()
{
    hideGrabbers();
    auto childs = childItems();
    for (auto& it : childs) {        
        removeItemFromGroup(it);
    }

    for(int i = 0; i < 4; i++){
        if (cornerGrabber[i] == nullptr) continue;
//        qDebug()<<"DEALLOCATE: "<< (void*)cornerGrabber[i];
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

void ItemGroup::dumpBits(QString text)
{
    auto childs = childItems();
    auto scene_tmp = childs.first()->sceneBoundingRect();

    for (auto& it : childs) {
        if (it->type() == eBorderDot) continue;
        scene_tmp = scene_tmp.united(it->sceneBoundingRect());
        auto widget = qgraphicsitem_cast<MoveItem*>(it);
        qDebug()<<"\tdumpBits:" << text;
        auto qimg = &widget->qimage();
        auto qimg_ptr = widget->qimage_ptr();
        void* ptr = (void*)(widget->qimage().constBits());
        void* ptr0 = (void*)widget->qimage_ptr()->constBits();
        const uchar* ptr1 = widget->qimage().constBits();
        const uchar* bits = widget->qimage().constBits();

        LOG_DEBUG(logger, "Adress: ", widget);
        LOG_DEBUG(logger, "@ qimage addr: ", qimg);
        LOG_DEBUG(logger, "@ qimageptr addr: ", qimg_ptr);
        LOG_DEBUG(logger, "@ bits addr: ", ptr);
        LOG_DEBUG(logger, "@ bits addr: ", ptr0);
        LOG_DEBUG(logger, "@ bits addr: ", (void*)ptr1);
        LOG_DEBUG(logger, "@ bits addr: ", (void*)bits);

        for (int i = 0; i < 20; ++i) {
            qDebug()<< bits[i];
        }
    }
}

QImage ItemGroup::mergedImages()
{
    QImage result(boundingRect().width(), boundingRect().height(), QImage::Format::Format_RGB32); // image to hold the join of image 1 & 2
    result.fill(QColor(42,42,42));
    QPainter painter(&result);

    qDebug()<<boundingRect();

    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);
        qDebug()<<widget->pos()<<' '<<widget->boundingRect();
        painter.drawImage(
                    {widget->pos() - boundingRect().topLeft(), widget->boundingRect().size()},
                    widget->qimage());
    }
    return result;
}

void ItemGroup::cloneItems()
{
    tmpitems_ = items_;
}

void ItemGroup::setScaleControlFactor(qreal sf)
{
    controlScaleFactor_ = sf;

    updateScaleControl();
}

void ItemGroup::updateScaleControl()
{
    if (!cornerGrabber[0]) return;
    cornerGrabber[0]->SetScale(1/controlScaleFactor_);
    cornerGrabber[1]->SetScale(1/controlScaleFactor_);
    cornerGrabber[2]->SetScale(1/controlScaleFactor_);
    cornerGrabber[3]->SetScale(1/controlScaleFactor_);
}

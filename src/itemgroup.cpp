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
    for(int i = 0; i < 4; i++){
        cornerGrabber[i] = new DotSignal(this);
    }
    setPositionGrabbers();
}

void ItemGroup::addItem(QGraphicsItem* item)
{
    LOG_DEBUG(logger, "ItemGroup::addItem: ", item);
    addToGroup(item);

    if (item->type() != eBorderDot) {
        items_.emplace_back(item);
    }

    auto childs = childItems();
    auto tmp = childs.first()->sceneBoundingRect();
    for (auto& it : childs) {
        if (it->type() == eBorderDot) continue;
        tmp = tmp.united(it->sceneBoundingRect());
    }
    rectItemGroup_ = tmp;

}

void ItemGroup::printChilds()
{
    auto childs = childItems();
    for (auto& it : childs) {
        LOG_DEBUG(logger, "CHILDREN: ", it);
    }
}

QRectF ItemGroup::boundingRect() const
{
    return rectItemGroup_;
}

void ItemGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pt = event->pos();
    if(m_actionFlags == ResizeState){
        switch (m_cornerFlags) {
//        case Top:
//            resizeTop(pt);
//            break;
//        case Bottom:
//            resizeBottom(pt);
//            break;
//        case Left:
//            resizeLeft(pt);
//            break;
//        case Right:
//            resizeRight(pt);
//            break;
        case TopLeft:
            resizeTopLeft(pt);
//            resizeTop(pt);
//            resizeLeft(pt);
            break;
        case TopRight:
            resizeTopRight(pt);
//            resizeTop(pt);
//            resizeRight(pt);
            break;
        case BottomLeft:
//            resizeBottom(pt);
//            resizeLeft(pt);
            break;
        case BottomRight:
//            resizeBottom(pt);
//            resizeRight(pt);
            break;
        default:
            if (m_leftMouseButtonPressed) {
                setCursor(Qt::ClosedHandCursor);
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
    qDebug()<<"ItemGroup::hoverEnterEvent";
    setPositionGrabbers();
    setVisibilityGrabbers();
    QGraphicsItem::hoverEnterEvent(event);
}

void ItemGroup::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    qDebug()<<"ItemGroup::hoverLeaveEvent";
    m_cornerFlags = 0;
    hideGrabbers();
    setCursor(Qt::CrossCursor);
    QGraphicsItem::hoverLeaveEvent( event );
}

void ItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - boundingRect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - boundingRect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - boundingRect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - boundingRect().bottom();   // Distance between the mouse and the bottom


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

void ItemGroup::resizeLeft(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    // if the mouse is on the right side we return
    if( pt.x() > tmpRect.right() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.right() );
    // limit the minimum width
    if( widthOffset > -10 )
        return;
    // if it's negative we set it to a positive width value
    if( widthOffset < 0 )
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    // Since it's a left side , the rectange will increase in size
    // but keeps the topLeft as it was
    tmpRect.translate( boundingRect().width() - tmpRect.width() , 0 );

    prepareGeometryChange();
    // Set the ne geometry
    rectItemGroup_ = tmpRect;
    // Update to see the result
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeRight(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    if( pt.x() < tmpRect.left() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 0)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );

    prepareGeometryChange();
    rectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeBottom(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    if( pt.y() < tmpRect.top() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.top() );
    if( heightOffset < 11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    prepareGeometryChange();
    rectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeTop(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    if( pt.y() > tmpRect.bottom() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.bottom() );
    if( heightOffset > -11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    tmpRect.translate( 0 , boundingRect().height() - tmpRect.height() );

    prepareGeometryChange();
    rectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeTopLeft(const QPointF &pt)
{
    auto pos = pt;
    auto rect = boundingRect();

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


    QRectF tmpRect = boundingRect();

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

    tmpRect.translate( boundingRect().width() - tmpRect.width() , boundingRect().height() - tmpRect.height() );
    prepareGeometryChange();


    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        qreal old_ig_w = rectItemGroup_.width();
        qreal old_ig_h = rectItemGroup_.height();
        qreal new_ig_w = tmpRect.width();
        qreal new_ig_h = tmpRect.height();

        qreal old_h = abs(widget->sceneBoundingRect().bottom() -  widget->sceneBoundingRect().top());
        qreal new_h = (old_h * new_ig_h) / old_ig_h;
        qreal old_w = abs(widget->sceneBoundingRect().left() - widget->sceneBoundingRect().right());
        qreal new_w = (old_w * new_h) / old_h;

        qreal new_x = 0;
        qreal new_y = 0;

        if (widget->sceneBoundingRect().left() == boundingRect().left()) {
            qreal aa = abs(widget->sceneBoundingRect().top() - rectItemGroup_.bottom());
            qreal yy = (aa*new_ig_w)/old_ig_w;
            qreal y_delta = abs(new_ig_h - yy);

            new_x = tmpRect.topLeft().x();
            new_y = tmpRect.topLeft().y() + y_delta;
        } else if (widget->sceneBoundingRect().top() == boundingRect().top()) {
            qreal aa = abs(widget->sceneBoundingRect().left() - rectItemGroup_.right());
            qreal xx = (aa*new_ig_h)/old_ig_h;
            qreal x_delta = abs(new_ig_w - xx);

            new_x = tmpRect.topLeft().x() + x_delta;
            new_y = tmpRect.topLeft().y();
        } else  {
            qreal qq = abs(rectItemGroup_.left() - widget->sceneBoundingRect().left());
            qreal ww = abs(rectItemGroup_.top() - widget->sceneBoundingRect().top());
            qreal new_x_delta = (new_ig_w * qq) / old_ig_w;
            qreal new_y_delta = (new_ig_h * ww) / old_ig_h;

            new_x = tmpRect.left() + new_x_delta;
            new_y = tmpRect.top() + new_y_delta;
        }

        widget->setX(new_x);
        widget->setY(new_y);
        widget->setRect(new_x, new_y, new_w, new_h);
    }

    rectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}

void ItemGroup::resizeTopRight(const QPointF &pt)
{
    auto pos = pt;
    auto rect = boundingRect();

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

    QRectF tmpRect = boundingRect();

    qreal heightOffset = ( pos.y() - rect.bottom() );
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );

    qreal widthOffset =  ( pos.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 0)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );

    tmpRect.translate( 0 , boundingRect().height() - tmpRect.height() );

    prepareGeometryChange();

    for (auto& it : items_) {
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        qreal old_ig_w = rectItemGroup_.width();
        qreal old_ig_h = rectItemGroup_.height();
        qreal new_ig_w = tmpRect.width();
        qreal new_ig_h = tmpRect.height();

        qreal old_h = abs(widget->sceneBoundingRect().bottom() -  widget->sceneBoundingRect().top());
        qreal new_h = (old_h * new_ig_h) / old_ig_h;
        qreal old_w = abs(widget->sceneBoundingRect().left() - widget->sceneBoundingRect().right());
        qreal new_w = (old_w * new_h) / old_h;

        qreal new_x = 0;
        qreal new_y = 0;

        if (widget->sceneBoundingRect().left() == boundingRect().left()) {
            qreal aa = abs(widget->sceneBoundingRect().top() - rectItemGroup_.bottom());
            qreal yy = (aa*new_ig_w)/old_ig_w;
            qreal y_delta = abs(new_ig_h - yy);

            new_x = tmpRect.topLeft().x();
            new_y = tmpRect.topLeft().y() + y_delta;
        } else if (widget->sceneBoundingRect().top() == boundingRect().top()) {
            qreal aa = abs(widget->sceneBoundingRect().left() - rectItemGroup_.right());
            qreal xx = (aa*new_ig_h)/old_ig_h;
            qreal x_delta = abs(new_ig_w - xx);

            new_x = tmpRect.topLeft().x() + x_delta;
            new_y = tmpRect.topLeft().y();
        } else  {
            qreal qq = abs(rectItemGroup_.left() - widget->sceneBoundingRect().left());
            qreal ww = abs(rectItemGroup_.top() - widget->sceneBoundingRect().top());
            qreal new_x_delta = (new_ig_w * qq) / old_ig_w;
            qreal new_y_delta = (new_ig_h * ww) / old_ig_h;

            new_x = tmpRect.left() + new_x_delta;
            new_y = tmpRect.top() + new_y_delta;
        }

        widget->setX(new_x);
        widget->setY(new_y);
        widget->setRect(new_x, new_y, new_w, new_h);
    }

    rectItemGroup_ = tmpRect;
    update();
    setPositionGrabbers();
}


void ItemGroup::rotateItem(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    QPointF center = boundingRect().center();
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
    QRectF tmpRect = boundingRect();
//    cornerGrabber[GrabberTop]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.top());
//    cornerGrabber[GrabberBottom]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.bottom());
//    cornerGrabber[GrabberLeft]->setPos(tmpRect.left(), tmpRect.top() + tmpRect.height()/2);
//    cornerGrabber[GrabberRight]->setPos(tmpRect.right(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberTopLeft]->setPos(tmpRect.topLeft().x(), tmpRect.topLeft().y());
    cornerGrabber[GrabberTopRight]->setPos(tmpRect.topRight().x(), tmpRect.topRight().y());
    cornerGrabber[GrabberBottomLeft]->setPos(tmpRect.bottomLeft().x(), tmpRect.bottomLeft().y());
    cornerGrabber[GrabberBottomRight]->setPos(tmpRect.bottomRight().x(), tmpRect.bottomRight().y());
}

void ItemGroup::setVisibilityGrabbers()
{
    cornerGrabber[GrabberTopLeft]->setVisible(true);
    cornerGrabber[GrabberTopRight]->setVisible(true);
    cornerGrabber[GrabberBottomLeft]->setVisible(true);
    cornerGrabber[GrabberBottomRight]->setVisible(true);

//    if(m_actionFlags == ResizeState){
//        cornerGrabber[GrabberTop]->setVisible(true);
//        cornerGrabber[GrabberBottom]->setVisible(true);
//        cornerGrabber[GrabberLeft]->setVisible(true);
//        cornerGrabber[GrabberRight]->setVisible(true);
//    } else {
//        cornerGrabber[GrabberTop]->setVisible(false);
//        cornerGrabber[GrabberBottom]->setVisible(false);
//        cornerGrabber[GrabberLeft]->setVisible(false);
//        cornerGrabber[GrabberRight]->setVisible(false);
//    }
}

void ItemGroup::hideGrabbers()
{
    for(int i = 0; i < 4; i++){
        cornerGrabber[i]->setVisible(false);
    }
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
    hideGrabbers();
    auto childs = childItems();
    for (auto& it : childs) {        
        if (it->type() != eBorderDot) {
            removeFromGroup(it);
            LOG_DEBUG(logger, "REMOVE ", it);
        }
    }
    rectItemGroup_ = QRectF();
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

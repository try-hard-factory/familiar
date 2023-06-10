#include "moveitem.h"
#include "debug_macros.h"
#include <QPen>


#include "Logger.h"
#include "Vec2d.h"

extern Logger logger;

#define MOUSE_MOVE_DEBUG

/*********************************************************************/


MoveItem::MoveItem(const QString& path, uint64_t& zc, QGraphicsRectItem* parent)
    : QGraphicsRectItem(parent)
    , zCounter_(zc)
{
    setZValue(zCounter_);
    //    qimage_ = QImage("bender.png");
    qimage_ = new QImage(path);
    //    auto qimage = QImage("kot.jpg");
    qInfo() << qimage_->width() << ' ' << qimage_->height()
            << " SIZE=" << qimage_->sizeInBytes();
    pixmap_ = QPixmap::fromImage(*qimage_);
    _size = pixmap_.size();
    rect_ = qimage_->rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

MoveItem::MoveItem(QImage* img, uint64_t& zc, QGraphicsRectItem* parent)
    : QGraphicsRectItem(parent)
    , qimage_(new QImage(*img))
    , zCounter_(zc)
{
    setZValue(zCounter_);

    pixmap_ = QPixmap::fromImage(*qimage_);
    //    LOG_DEBUG(logger, "qimage addr: ", &qimage_);
    //    LOG_DEBUG(logger, "pixmap addr: ", &pixmap_);
    //    void* ptr = (void*)qimage_.bits();
    //    LOG_DEBUG(logger, "bits addr: ", ptr);

    _size = pixmap_.size();
    rect_ = qimage_->rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

MoveItem::MoveItem(QByteArray ba,
                   int w,
                   int h,
                   qsizetype bpl,
                   QImage::Format f,
                   uint64_t& zc,
                   QGraphicsRectItem* parent)
    : QGraphicsRectItem(parent)
    , ba_(ba)
    , zCounter_(zc)
{
    setZValue(zCounter_);
    const uchar* bits = (const unsigned char*) ba_.data();
    qimage_ = new QImage(bits, w, h, bpl, f);
    pixmap_ = QPixmap::fromImage(*qimage_);

    _size = pixmap_.size();
    rect_ = qimage_->rect();

    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

MoveItem::~MoveItem()
{
    delete qimage_;
}


QRectF MoveItem::boundingRect() const
{
    return QRectF(0, 0, rect_.width(), rect_.height());
}


QPainterPath MoveItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}


void MoveItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget)
{
    painter->drawImage(boundingRect(), *qimage_);
    qreal wsize = 2;
    QPen outline_pen{QColor(22, 142, 153), wsize};
    outline_pen.setCosmetic(true);
    painter->setPen(outline_pen);

    if (inGroup_) {
        QPainterPath path;
        path.addPath(shape());

        painter->drawPath(path);
    }


    Q_UNUSED(widget);
}

void MoveItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    LOG_DEBUG(logger,
              "EventPos: (",
              event->pos().x(),
              ";",
              event->pos().y(),
              "), Pos: (",
              pos().x(),
              ";",
              pos().y(),
              ")");

    QGraphicsItem::mouseMoveEvent(event);
}

void MoveItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    setZValue(++zCounter_);

    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos())) / scale();

    LOG_DEBUG(logger,
              "EventPos: (",
              event->pos().x(),
              ";",
              event->pos().y(),
              "), Pos: (",
              pos().x(),
              ";",
              pos().y(),
              ")");

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
            //            setSelected(!isSelected());
        } else {
            QGraphicsItem::mousePressEvent(event);
        }
    }
}

void MoveItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    LOG_DEBUG(logger,
              "EventPos: (",
              event->pos().x(),
              ";",
              event->pos().y(),
              "), Pos: (",
              pos().x(),
              ";",
              pos().y(),
              ")");

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
            //            setSelected(!isSelected());
        } else {
            QGraphicsItem::mousePressEvent(event);
        }
    }
}

void MoveItem::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    /*Scale 0.2 each turn of the wheel (which is usually 120.0 eights of degrees)*/
    qreal scaleFactor = 1.15; //1.0 + event->delta() * 0.2 / 120.0;
    if (event->delta() > 0)
        setScale(scale() * scaleFactor);
    else
        setScale(scale() * (1 / scaleFactor));
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
    setRect(QRectF(x, y, w, h));
}

void MoveItem::setRect(const QRectF& rect)
{
    rect_ = rect;
    QGraphicsRectItem::setRect(rect);
}

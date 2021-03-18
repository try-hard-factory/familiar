#include "movepixmapitem.h"

MovePixmapItem::MovePixmapItem(QGraphicsItem *parent) :
    QGraphicsPixmapItem(parent)
{
    auto qimage = QImage("kot.png");
    qInfo() << qimage.width() << ' ' <<qimage.height();
    pixmap_ = QPixmap::fromImage(qimage);
     qInfo() << pixmap_.width() << ' ' <<pixmap_.height();
     setPixmap(pixmap_);
}
MovePixmapItem::~MovePixmapItem()
{

}

QRectF MovePixmapItem::boundingRect() const
{
    return QRectF (0,0,pixmap_.width(),pixmap_.height());
}

void MovePixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::black);
//    painter->setBrush(Qt::green);
    painter->drawRect(0,0,pixmap_.width(),pixmap_.height());
    QPointF point(0, 0);
    QRectF source(0, 0, pixmap_.width(),pixmap_.height());
    painter->drawPixmap(point, pixmap_, source);
    Q_UNUSED(option);
    Q_UNUSED(widget);

}

void MovePixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
     qInfo() << "mouseMoveEvent";
    /* Устанавливаем позицию графического элемента
     * в графической сцене, транслировав координаты
     * курсора внутри графического элемента
     * в координатную систему графической сцены
     * */
    this->setPos(mapToScene(event->pos() + shiftMouseCoords_));
//    QGraphicsPixmapItem::mouseMoveEvent(event);
}

void MovePixmapItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo() << "mousePressEvent";
    /* При нажатии мышью на графический элемент
     * заменяем курсор на руку, которая держит этот элемента
     * */

    shiftMouseCoords_ = (this->pos() - mapToScene(event->pos()))/scale();
    this->setCursor(QCursor(Qt::ClosedHandCursor));
    Q_UNUSED(event);
//    QGraphicsPixmapItem::mouseMoveEvent(event);
}

void MovePixmapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo() << "mouseReleaseEvent";
    /* При отпускании мышью элемента
     * заменяем на обычный курсор стрелку
     * */
    this->setCursor(QCursor(Qt::ArrowCursor));
    Q_UNUSED(event);
//    QGraphicsPixmapItem::mouseMoveEvent(event);
}

void MovePixmapItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
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

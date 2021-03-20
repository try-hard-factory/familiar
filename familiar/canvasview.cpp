#include "canvasview.h"
#include "moveitem.h"
#include "movepixmapitem.h"
#include "canvasscene.h"
//#define MOUSE_MOVE_DEBUG
CanvasView::CanvasView(QWidget* parent) :
    QGraphicsView(parent)
{
    zoomFactor_ = ZOOM_FACTOR;
    scene_ = new CanvasScene(zCounter_);
    connect(scene_, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
//    scene_->setItemIndexMethod(QGraphicsScene::NoIndex);
    setMouseTracking(true);
    setScene(scene_);

    // Update all the view port when needed, otherwise, the drawInViewPort may experience trouble
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    // When zooming, the view stay centered over the mouse
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::RubberBandDrag);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}


CanvasView::~CanvasView()
{
    delete scene_;
}


void CanvasView::addImage(const QImage &image, QPointF point)
{
    ++zCounter_;
    MoveItem* item = new MoveItem(zCounter_);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
//    item->setFlag(QGraphicsItem::ItemIsMovable, true);
//    item->setFlag(QGraphicsItem::ItemIsFocusable, true);
//    MovePixmapItem* item = new MovePixmapItem();

    item->setPos(point);
    scene_->addItem(item);
    //    QPixmap myPixmap("kot.png"); // создаем картинку
    //    QGraphicsPixmapItem *item = new QGraphicsPixmapItem(myPixmap);
}


void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    qInfo()<<"CanvasView::mouseMoveEvent: "<<event->position();
#endif
    if (pan_) {
//        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (event->position().x() - panStartX_));
//        verticalScrollBar()->setValue(verticalScrollBar()->value() - (event->position().y() - panStartY_));
        panStartX_ = event->position().x();
        panStartY_ = event->position().y();
        event->accept();
//        return;
    }
//    event->ignore();

    // Get the coordinates of the mouse in the scene
//    QPointF imagePoint = mapToScene(QPoint(event->position().x(), event->position().y() ));
//    // Call the function that create the tool tip
//    setToolTip(setToolTipText(QPoint((int)imagePoint.x(),(int)imagePoint.y())));
    // Call the parent's function (for dragging)
    QGraphicsView::mouseMoveEvent(event);
}


void CanvasView::mousePressEvent(QMouseEvent *event)
{
//    qInfo()<<"CanvasView::mousePressEvent: "<<event->position();
    if (event->button() == Qt::LeftButton) {
         pan_ = true;
         panStartX_ = event->position().x();
         panStartY_ = event->position().y();
         event->accept();

     }
    //    if (event->button() == Qt::RightButton) this->fitImage();
    QGraphicsView::mousePressEvent(event);
}


void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
//    qInfo()<<"CanvasView::mouseReleaseEvent: "<<event->position();

    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        pan_ = false;
        event->accept();
      }
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    // When zooming, the view stay centered over the mouse
    this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    auto numPixels = event->angleDelta();
//    qInfo()<<"CanvasView::wheelEvent: "<<numPixels.x()<<' '<<numPixels.y();
    double factor = zoomFactor_;//(event->modifiers() & Qt::ControlModifier) ? zoomCtrlFactor : zoomFactor;
    if (numPixels.y() > 0) {
        scale(factor, factor);
    } else {
        scale(1/factor, 1/factor);
    }

    // The event is processed
    event->accept();
}


void CanvasView::resizeEvent(QResizeEvent *event)
{
    // First call, the scene is created
    if(event->oldSize().width() == -1 || event->oldSize().height() == -1) return;

    // Get the previous rectangle of the scene in the viewport
    QPointF P1=mapToScene(QPoint(0,0));
    QPointF P2=mapToScene(QPoint(event->oldSize().width(),event->oldSize().height()));

    // Stretch the rectangle around the scene
    if (P1.x()<0) P1.setX(0);
    if (P1.y()<0) P1.setY(0);
    if (P2.x()>scene_->width()) P2.setX(scene_->width());
    if (P2.y()>scene_->height()) P2.setY(scene_->height());
}


QString CanvasView::setToolTipText(QPoint imageCoordinates)
{
    (void)imageCoordinates;
    return QString("");

}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    setCacheMode(CacheNone);
    painter->save();
    painter->setPen( QPen(Qt::darkGray,1) );
    painter->drawRect(scene_->sceneRect());
    painter->restore();
}

void CanvasView::onSelectionChanged()
{
    scene_->onSelectionChanged();
}

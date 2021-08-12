#include "canvasview.h"
#include "moveitem.h"
#include "movepixmapitem.h"
#include "canvasscene.h"
#include <QFileDialog>
#include <QMessageBox>
#include "Logger.h"
#include <QtMath>
extern Logger logger;


CanvasView::CanvasView(QWidget* parent) :
    QGraphicsView(parent)
{
    scene_ = new CanvasScene(zCounter_);
    connect(scene_, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

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


void CanvasView::openFile()
{

}


void CanvasView::saveAsFile()
{

}


std::string CanvasView::fml_header()
{
    return scene_->fml_header();
}


QByteArray CanvasView::fml_payload()
{
    return scene_->fml_payload();
}


void CanvasView::addImage(const QString& path, QPointF point)
{
    ++zCounter_;
    MoveItem* item = new MoveItem(path, zCounter_);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    item->setPos(point);
    scene_->addItem(item);
}


void CanvasView::addImage(QImage* img, QPointF point)
{
    ++zCounter_;
    MoveItem* item = new MoveItem(img, zCounter_);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    item->setPos(point);
    scene_->addItem(item);
}

void CanvasView::addImage(QByteArray ba, int w, int h, QRect br, qsizetype bpl, QImage::Format f)
{
    ++zCounter_;
    MoveItem* item = new MoveItem(ba, w, h, bpl, f, zCounter_);
    item->setRect(br);
    LOG_DEBUG(logger, "Adress: ", item, ", Z: ", item->zValue());
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    item->setPos(br.topLeft());
    scene_->addItem(item);
}


void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (pan_) {
        panStartX_ = event->position().x();
        panStartY_ = event->position().y();
        event->accept();
    }

    QGraphicsView::mouseMoveEvent(event);
}


void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        pan_ = true;
        panStartX_ = event->position().x();
        panStartY_ = event->position().y();
        event->accept();
    }
    QGraphicsView::mousePressEvent(event);
}


void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        pan_ = false;
        event->accept();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::ScaleView(qreal qFactor)
{
    QTransform matrix = this->transform();
    qreal qrCurrTx = 0.0, qCurrTy = 0.0;
    matrix.map(1.0, 1.0, &qrCurrTx, &qCurrTy);
    double dbCurrentFactor = (qrCurrTx + qCurrTy) * 0.5;

    qreal qrNewTx = 0.0, qrNewTy = 0.0;
    matrix.scale(qFactor, qFactor).map(1.0, 1.0, &qrNewTx, &qrNewTy);
    double dbNewFactor = (qrNewTx + qrNewTy) * 0.5;

    if ((0.07 <= dbNewFactor <= 100.0) ||
        ((dbCurrentFactor < 0.07) && (dbNewFactor > dbCurrentFactor)) ||
        ((dbCurrentFactor > 100.0) && (dbNewFactor < dbCurrentFactor)))
    {
        scale(qFactor, qFactor);
//        emit SetZoomText(qFactor);
    }
}

void CanvasView::SetScale(qreal qrScale)
{
    QTransform old_matrix = this->transform();
    resetTransform();
    translate(old_matrix.dx(), old_matrix.dy());
    scale(qrScale, qrScale);
//    emit SetZoomText(qrScale);
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    // When zooming, the view stay centered over the mouse
    this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
//    qreal qrValue = qPow(2.0, (event->angleDelta().y() / 240.0));

//    ScaleView(qrValue);
    auto numPixels = event->angleDelta();
    double factor = zoomFactor_;//(event->modifiers() & Qt::ControlModifier) ? zoomCtrlFactor : zoomFactor;
    if (numPixels.y() > 0) {
        scale(factor, factor);
    } else {
        scale(1/factor, 1/factor);
    }


    scene_->updateViewScaleFactor(transform().m11());

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


void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    setCacheMode(CacheNone);
    painter->save();
    setBackgroundBrush(QBrush(QColor(32, 32, 32)));
    painter->fillRect(rect, backgroundBrush());
    scene_->setBackgroundBrush(QBrush(QColor(42, 42, 42)));
    painter->fillRect(scene_->sceneRect(), scene_->backgroundBrush());
    painter->setPen( QPen(QColor(247, 0, 255),2) );
    painter->drawRect(scene_->sceneRect());
    painter->restore();
}


void CanvasView::onSelectionChanged()
{
    scene_->onSelectionChanged();
}

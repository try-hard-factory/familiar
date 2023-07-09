#include "canvasview.h"
#include "Logger.h"
#include "canvasscene.h"
#include "moveitem.h"
#include "movepixmapitem.h"
#include "project_settings.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QtMath>

#include "mainwindow.h"
#include <main_context_menu.h>

extern Logger logger;


CanvasView::CanvasView(MainWindow& mw, QWidget* parent)
    : QGraphicsView(parent)
    , mainwindow_(mw)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scene_ = new CanvasScene(mw, zCounter_);

    connect(scene_, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &CanvasView::settingsChangedSlot);

    settingsChangedSlot();
    setMouseTracking(true);
    setScene(scene_);

    // Update all the view port when needed, otherwise, the drawInViewPort may experience trouble
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // When zooming, the view stay centered over the mouse
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::RubberBandDrag);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    //setAttribute(Qt::WA_TranslucentBackground);
}


CanvasView::~CanvasView()
{
    delete scene_;
}

void CanvasView::setProjectSettings(project_settings* ps)
{
    scene_->setProjectSettings(ps);
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


void CanvasView::addNote(const QString& note)
{
    ++zCounter_;
    TextItem* item = new TextItem();
    item->setText(note);
    //item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    item->setPos(lastClickedPoint_);
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
    // LOG_DEBUG(logger, "Adress: ", item, ", Z: ", item->zValue());
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    item->setPos(br.topLeft());
    scene_->addItem(item);
}


void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (pan_ == Qt::RightButton) {
        rightMoveflag_ = true;
        if (isMoving_) {
            mainwindow_.move(wndPos_ + (event->globalPosition().toPoint() - pressPos_));
        }
        event->accept();
    } else if (pan_ == Qt::LeftButton) {
        event->accept();
    }

    QGraphicsView::mouseMoveEvent(event);
}


void CanvasView::mousePressEvent(QMouseEvent* event)
{
    qDebug() << "CanvasView::mousePressEvent";
    if (event->button() == Qt::LeftButton) {
        pan_ = Qt::LeftButton;
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        setDragMode(QGraphicsView::NoDrag);
        pan_ = Qt::RightButton;
        pressPos_ = event->globalPosition().toPoint();
        isMoving_ = true;
        wndPos_ = mainwindow_.pos();
        event->accept();
    }
    QGraphicsView::mousePressEvent(event);
}


void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    lastClickedPoint_ = event->pos();

    if (event->button() == Qt::RightButton) {
        if (!rightMoveflag_) {
            MainContextMenu contextMenu(mainwindow_, this);
            QPointF POS = mapToGlobal(event->pos());
            contextMenu.exec(POS.toPoint());
        } else {
            rightMoveflag_ = false;
        }
        isMoving_ = false;
        setDragMode(QGraphicsView::RubberBandDrag);
        event->accept();
    } else if (event->button() == Qt::LeftButton) {
        event->accept();
    }

    pan_ = Qt::NoButton;
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    qDebug() << "CanvasView::mouseDoubleClickEvent";
    // if (scene_->itemGroup()->containsOnlyNote()) {
    //     scene_->itemGroup()->startEditNote();
    //     QGraphicsView::mouseDoubleClickEvent(event);
    //     return;
    // }
    // if (!fitViewF_) {
    //     fitViewF_ = true;
    //     fitInView(scene_->itemGroup(), Qt::KeepAspectRatio);
    //     scene_->updateViewScaleFactor(transform().m11());
    // } else {
    //     fitViewF_ = false;
    //     fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    //     scene_->updateViewScaleFactor(transform().m11());
    // }
    QGraphicsView::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton) {
        event->accept();
    }
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

    if (((0.07 <= dbNewFactor) && (dbNewFactor <= 100.0))
        || ((dbCurrentFactor < 0.07) && (dbNewFactor > dbCurrentFactor))
        || ((dbCurrentFactor > 100.0) && (dbNewFactor < dbCurrentFactor))) {
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

void CanvasView::wheelEvent(QWheelEvent* event)
{
    // When zooming, the view stay centered over the mouse
    this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    //    qreal qrValue = qPow(2.0, (event->angleDelta().y() / 240.0));

    //    ScaleView(qrValue);
    auto numPixels = event->angleDelta();
    double factor
        = zoomFactor_; //(event->modifiers() & Qt::ControlModifier) ? zoomCtrlFactor : zoomFactor;
    if (numPixels.y() > 0) {
        scale(factor, factor);
    } else {
        scale(1 / factor, 1 / factor);
    }


    scene_->updateViewScaleFactor(transform().m11());

    // The event is processed
    event->accept();
}


void CanvasView::resizeEvent(QResizeEvent* event)
{
    // First call, the scene is created
    if (event->oldSize().width() == -1 || event->oldSize().height() == -1)
        return;

    // Get the previous rectangle of the scene in the viewport
    QPointF P1 = mapToScene(QPoint(0, 0));
    QPointF P2 = mapToScene(QPoint(event->oldSize().width(), event->oldSize().height()));

    // Stretch the rectangle around the scene
    if (P1.x() < 0)
        P1.setX(0);
    if (P1.y() < 0)
        P1.setY(0);
    if (P2.x() > scene_->width())
        P2.setX(scene_->width());
    if (P2.y() > scene_->height())
        P2.setY(scene_->height());
}


void CanvasView::drawBackground(QPainter* painter, const QRectF& rect)
{
    // TODO:
    int local_opacity = currentOpacity_;
    //if (local_opacity < 255) local_opacity -=100;
    qreal opacity = (qreal) local_opacity / 255;
    painter->setOpacity(opacity);
    setCacheMode(CacheNone);
    painter->save();
    // setBackgroundBrush(QBrush(QColor(32, 255, 32)));
    // painter->fillRect(rect, backgroundBrush());
    scene_->setBackgroundBrush(QBrush(canvasColor_));
    painter->fillRect(scene_->sceneRect(), scene_->backgroundBrush());
    painter->setPen(QPen(borderColor_, 2));
    painter->drawRect(scene_->sceneRect());
    painter->restore();
}

void CanvasView::cleanupWorkplace()
{
    scene_->cleanupWorkplace();
}

QString CanvasView::path()
{
    return scene_->path();
}

void CanvasView::setPath(const QString& path)
{
    scene_->setPath(path);
}

QString CanvasView::projectName()
{
    return scene_->projectName();
}

void CanvasView::setProjectName(const QString& pn)
{
    scene_->setProjectName(pn);
}

bool CanvasView::isModified()
{
    return scene_->isModified();
}

void CanvasView::setModified(bool mod)
{
    scene_->setModified(mod);
}

bool CanvasView::isUntitled()
{
    return scene_->isUntitled();
}


void CanvasView::onSelectionChanged()
{
    scene_->onSelectionChanged();
}

void CanvasView::settingsChangedSlot()
{
    auto settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    canvasColor_ = colorPreset[EPresetsColorIdx::kCanvasColor];
    borderColor_ = colorPreset[EPresetsColorIdx::kBorderColor];
    currentOpacity_ = settings->getCurrentOpacity();
}

void CanvasView::contextMenuEvent(QContextMenuEvent* event)
{
    //    MainContextMenu contextMenu(mainwindow_, this);
    //    contextMenu.exec(event->globalPos());
}

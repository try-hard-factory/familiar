#include "canvasview.h"
#include "Logger.h"
#include "canvasscene.h"
#include "moveitem.h"
#include "project_settings.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QUndoStack>
#include <QtMath>

#include "mainwindow.h"
#include <main_context_menu.h>

extern Logger logger;


CanvasView::CanvasView(MainWindow& mw, QWidget* parent)
    : MainControlsMixin<CanvasView, QGraphicsView>()
    // , QGraphicsView(parent)
    // , ActionsMixin<CanvasView>()
    , mainwindow_(mw)
    , welcomeOverlay_(new WelcomeOverlay(this))
    , undoStack_(std::make_unique<QUndoStack>(this))
{
    setFrameShape(QFrame::NoFrame);

    undoStack_->setUndoLimit(100);
    // TODOLATER:
    // connect(undoStack_, &QUndoStack::canRedoChanged, this, &CanvasView::on_can_redo_changed);
    // connect(undoStack_, &QUndoStack::canUndoChanged, this, &CanvasView::on_can_undo_changed);
    // connect(undoStack_, &QUndoStack::cleanChanged, this, &CanvasView::on_undo_clean_changed);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scene_ = new CanvasScene(mw, zCounter_, undoStack_.get());
    connect(scene_, &CanvasScene::changed, this, &CanvasView::on_scene_changed);
    connect(scene_,
            &CanvasScene::selectionChanged,
            this,
            &CanvasView::on_selection_changed);
    // TODOLATER:
    // connect(scene_, &CanvasScene::cursor_changed, this, &CanvasView::on_cursor_changed);
    // connect(scene_, &CanvasScene::cursor_cleared, this, &CanvasView::on_cursor_cleared);
    setScene(scene_);

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &CanvasView::settingsChangedSlot);

    settingsChangedSlot();
    setMouseTracking(true);

    init_main_controls();

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
    delete welcomeOverlay_;
}


void CanvasView::on_scene_changed()
{
    if (scene_->items().isEmpty()) {
        //qDebug() << "No items in scene";
        setTransform(QTransform());
        welcomeOverlay_->setFocus();
        clearFocus();
        welcomeOverlay_->show();
        // TODOLATER:
        // actiongroup_set_enabled("active_when_items_in_scene", false);
    } else {
        setFocus();
        welcomeOverlay_->clearFocus();
        welcomeOverlay_->hide();
        // TODOLATER:
        // actiongroup_set_enabled("active_when_items_in_scene", true);
    }
    recalcSceneRect();
}

void CanvasView::setProjectSettings(project_settings* ps)
{
    scene_->setProjectSettings(ps);
}


QByteArray CanvasView::fml_payload()
{
    return scene_->fml_payload();
}

void CanvasView::do_insert_images(const QList<QUrl>& urls, const QPoint& pos)
{
    // TODOLATER: implement full insert images logic with undo stack
    // For now, just load images and add them to scene
    QPointF scenePos = mapToScene(pos);

    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            QString path = url.toLocalFile();
            addImage(path, scenePos);
        } else {
            // TODOLATER: handle remote URLs via ImageDownloader
            qDebug() << "Remote URL not yet supported:" << url;
        }
    }
}

void CanvasView::addImage(const QString& path, QPointF point)
{
    // ++zCounter_;
    // MoveItem* item = new MoveItem(path, zCounter_);
    // item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    // item->setPos(point);
    // scene_->addItem(item);
}


void CanvasView::addImage(QImage* img, QPointF point)
{
    // ++zCounter_;
    // MoveItem* item = new MoveItem(img, zCounter_);
    // item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    // item->setPos(point);
    // scene_->addItem(item);
}

void CanvasView::addImage(
    QByteArray ba, int w, int h, QRect br, qsizetype bpl, QImage::Format f)
{
    // ++zCounter_;
    // MoveItem* item = new MoveItem(ba, w, h, bpl, f, zCounter_);
    // item->setRect(br);
    // LOG_DEBUG(logger, "Adress: ", item, ", Z: ", item->zValue());
    // item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    // item->setPos(br.topLeft());
    // scene_->addItem(item);
}


void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (pan_ == Qt::RightButton) {
        rightMoveflag_ = true;
        if (isMoving_) {
            mainwindow_.move(wndPos_
                             + (event->globalPosition().toPoint() - pressPos_));
        }
        event->accept();
    } else if (pan_ == Qt::LeftButton) {
        panStartX_ = event->position().x();
        panStartY_ = event->position().y();
        event->accept();
    }

    QGraphicsView::mouseMoveEvent(event);
}


void CanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        pan_ = Qt::LeftButton;
        panStartX_ = event->position().x();
        panStartY_ = event->position().y();
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        setDragMode(QGraphicsView::NoDrag);
        pan_ = Qt::RightButton;
        panStartX_ = event->position().x();
        panStartY_ = event->position().y();
        pressPos_ = event->globalPosition().toPoint();
        isMoving_ = true;
        wndPos_ = mainwindow_.pos();
        event->accept();
    }
    QGraphicsView::mousePressEvent(event);
}


void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
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
    // // When zooming, the view stay centered over the mouse
    // this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    // //    qreal qrValue = qPow(2.0, (event->angleDelta().y() / 240.0));

    // //    ScaleView(qrValue);
    // auto numPixels = event->angleDelta();
    // double factor
    //     = zoomFactor_; //(event->modifiers() & Qt::ControlModifier) ? zoomCtrlFactor : zoomFactor;
    // if (numPixels.y() > 0) {
    //     scale(factor, factor);
    // } else {
    //     scale(1 / factor, 1 / factor);
    // }


    // scene_->updateViewScaleFactor(transform().m11());

    // // The event is processed
    // event->accept();
}


void CanvasView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);

    // First call, the scene is created
    if (event->oldSize().width() == -1 || event->oldSize().height() == -1)
        return;

    // Get the previous rectangle of the scene in the viewport
    QPointF P1 = mapToScene(QPoint(0, 0));
    QPointF P2 = mapToScene(
        QPoint(event->oldSize().width(), event->oldSize().height()));

    // Stretch the rectangle around the scene
    if (P1.x() < 0)
        P1.setX(0);
    if (P1.y() < 0)
        P1.setY(0);
    if (P2.x() > scene_->width())
        P2.setX(scene_->width());
    if (P2.y() > scene_->height())
        P2.setY(scene_->height());

    recalcSceneRect();
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

void CanvasView::on_selection_changed()
{
    // scene_->onSelectionChanged();
    qDebug() << "Currently selected items:" << scene()->selectedItems().size();
    // bool hasSelection = !scene->selectedItems().isEmpty();
    // actiongroup_set_enabled("active_when_selection", hasSelection);
    // actiongroup_set_enabled("active_when_croppable", scene->has_croppable_selection());
    // viewport()->repaint();
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

void CanvasView::resetPreviousTransform(QGraphicsItem* toggleItem)
{
    if (previousTransform_ && previousTransform_->toggleItem != toggleItem) {
        previousTransform_.reset();
    }
}

QPointF CanvasView::getViewCenter() const
{
    return QPointF(round(size().width() / 2.0), round(size().height() / 2.0));
}

void CanvasView::recalcSceneRect()
{
    // Resize the scene rectangle so that it is always one view width
    // wider than all items' bounding box at each side and one view
    // width higher on top and bottom. This gives the impression of
    // an infinite canvas.

    if (previousTransform_) {
        return;
    }

    QRectF itemsRect = scene_->itemsBoundingRect();
    if (itemsRect.isEmpty()) {
        return;
    }

    QPoint topleft = mapFromScene(itemsRect.topLeft());
    topleft = mapToScene(QPoint(topleft.x() - size().width(),
                                topleft.y() - size().height()))
                  .toPoint();
    QPoint bottomright = mapFromScene(itemsRect.bottomRight());
    bottomright = mapToScene(QPoint(bottomright.x() + size().width(),
                                    bottomright.y() + size().height()))
                      .toPoint();
    setSceneRect(QRectF(topleft, bottomright));
}

void CanvasView::fitRect(const QRectF& rect, QGraphicsItem* toggleItem)
{
    // Если есть toggleItem и есть сохраненная трансформация - восстанавливаем
    if (toggleItem && previousTransform_) {
        qDebug() << "Fit view: Reset to previous";
        setTransform(previousTransform_->transform);
        centerOn(previousTransform_->center);
        previousTransform_.reset();
        return;
    }

    // Сохраняем текущее состояние если есть toggleItem
    if (toggleItem) {
        previousTransform_ = std::make_unique<PreviousTransform>();
        previousTransform_->toggleItem = toggleItem;
        previousTransform_->transform = QTransform(transform());
        previousTransform_->center = mapToScene(getViewCenter().toPoint());
    } else {
        previousTransform_.reset();
    }

    qDebug() << "Fit view:" << rect;
    fitInView(rect, Qt::KeepAspectRatio);
    recalcSceneRect();
    // Второй вызов для надежности (как в Python)
    // Иногда изменение размера сцены может испортить fitting
    fitInView(rect, Qt::KeepAspectRatio);
    qDebug() << "Fit view done";
}

#include "debug_macros.h"
#include "canvasscene.h"

#include <QKeyEvent>
#include <QGraphicsItem>
#include <QPen>
#include <QPainter>

//#define MOUSE_MOVE_DEBUG
CanvasScene::CanvasScene(uint64_t& zc, QGraphicsScene *scene) : zCounter_(zc)
{
    (void)scene;
    itemGroup_ = new ItemGroup(zc);
    addItem(itemGroup_);
}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        qDebug() << "Selected items "<< selectedItems().size();
        for (auto it : selectedItems()) {
            removeItem(it);
        }
    } else {
        QGraphicsScene::keyPressEvent(event);
    }
}


void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QDEBUG<<"Event->scenePos: "<<event->scenePos();
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() != Qt::ShiftModifier) {
            if (!isAnySelectedUnderCursor()) {
                clearSelection();
                clearItemGroup();
            }
        }
    }
    QGraphicsScene::mousePressEvent(event);
}


void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QDEBUG<<"Event->scenePos: "<<event->scenePos();
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() != Qt::ShiftModifier) {
            if (isAnySelectedUnderCursor()) clearSelection();
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}


void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    qDebug()<<Q_FUNC_INFO<<"| Event->scenePos: "<<event->scenePos();
#endif


    QGraphicsScene::mouseReleaseEvent(event);
}


bool CanvasScene::isAnySelectedUnderCursor() const
{
    auto selItems = selectedItems();
    QDEBUG<<"Selected Items = "<<selItems.size();
    for (auto& it : selItems) {
        if (it->isUnderMouse()) return true;
    }

    return false;
}


void CanvasScene::clearItemGroup()
{
    auto childs = itemGroup_->childItems();
    for (auto& it : childs) {
        itemGroup_->removeFromGroup(it);
    }
}


void CanvasScene::onSelectionChanged()
{
    clearItemGroup();

    auto selItems = selectedItems();
//    QRectF result;
    for (auto& it : selItems) {
        itemGroup_->addToGroup(it);
//        result = result.united(it->boundingRect());
    }

//    mainSelArea_.setReady(selItems.empty());
//    mainSelArea_.setRect(result);
//    addRect(itemGroup_->childrenBoundingRect(), QPen(Qt::black, 2));

    itemGroup_->setZValue(++zCounter_);
}


void CanvasScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    painter->save();
    painter->setPen( QPen(Qt::black, 2) );
    painter->drawRect(itemGroup_->sceneBoundingRect());
    painter->restore();
}

#include "debug_macros.h"
#include "canvasscene.h"

#include <QKeyEvent>
#include <QGraphicsItem>
#include <QPen>
#include <QPainter>

#include "Logger.h"

extern Logger logger;


//#define MOUSE_MOVE_DEBUG
CanvasScene::CanvasScene(uint64_t& zc, QGraphicsScene *scene) : zCounter_(zc)
{
    (void)scene;
    itemGroup_ = new ItemGroup(zc);
    itemGroup_->setHandlesChildEvents(true);
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
    LOG_DEBUG(logger, "Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");

    auto item = getFirstItemUnderCursor(event->scenePos());

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {


            if (item) {
                item->setZValue(++zCounter_);
                itemGroup_->addToGroup(item);
                mainSelArea_.setReady(true);
            }

        } else if (event->modifiers() != Qt::ShiftModifier) {
            if (item) {
                if (!itemGroup_->isContain(item)) {
                    itemGroup_->clearItemGroup();
                    itemGroup_->addToGroup(item);
                    mainSelArea_.setReady(true);
                }                
            } else {
                itemGroup_->clearItemGroup();
                mainSelArea_.setReady(false);
            }
        }
    }
    QGraphicsScene::mousePressEvent(event);
}


void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    LOG_DEBUG(logger, "Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");

    auto item = getFirstItemUnderCursor(event->scenePos());

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() != Qt::ShiftModifier) {

            if (item) {

                if (!isGroupMoving) {
                    itemGroup_->clearItemGroup();
                    isGroupMoving = false;
                    item->setZValue(++zCounter_);
                    itemGroup_->addToGroup(item);
                    mainSelArea_.setReady(true);
                }

            } else {
                itemGroup_->clearItemGroup();
                mainSelArea_.setReady(false);
            }
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}


void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    LOG_DEBUG(logger, "Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");
#endif
    if ( (event->buttons() & Qt::LeftButton) &&
         itemGroup_->isUnderMouse()) {
        isGroupMoving = true;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

QGraphicsItem* CanvasScene::getFirstItemUnderCursor(const QPointF& p) {
    auto childs = this->items(p);
    for (auto& it : childs) {
        if (it != itemGroup_) return it;
    }

    return nullptr;
}


bool CanvasScene::isAnySelectedUnderCursor() const
{
    auto selItems = selectedItems();
    LOG_DEBUG(logger, "Selected Items: ", selItems.size());

    for (auto& it : selItems) {
        if (it->isUnderMouse()) return true;
    }

    return false;
}


void CanvasScene::onSelectionChanged()
{
//    clearItemGroup();

    auto selItems = selectedItems();
    LOG_DEBUG(logger, "Selected Items: ", selItems.size());
//    QRectF result;
//    for (auto& it : selItems) {
//        itemGroup_->addToGroup(it);
//        result = result.united(it->boundingRect());
//    }


//    QDEBUG<<" 2 Selected Items = "<<selItems.size();
//    mainSelArea_.setReady(selItems.empty());
//    mainSelArea_.setRect(result);
//    addRect(itemGroup_->childrenBoundingRect(), QPen(Qt::black, 2));

//    itemGroup_->setZValue(++zCounter_);
}


void CanvasScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    if (!mainSelArea_.isReady()) return;
    painter->save();
    painter->setPen( QPen(Qt::black, 2) );
    auto r = itemGroup_->sceneBoundingRect();
    painter->drawRect(r);
    painter->setPen( QPen(Qt::black, 10) );
    painter->drawPoint(r.x(), r.y());
    painter->drawPoint(r.x()+r.width(), r.y());
    painter->drawPoint(r.x()+r.width(), r.y()+r.height());
    painter->drawPoint(r.x(), r.y() + r.height());
    painter->restore();
}

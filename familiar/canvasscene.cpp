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
                state_ = eMouseSelection;
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
        if (event->modifiers() == Qt::ShiftModifier) {
            if (item) { // add to group
                if (!itemGroup_->isContain(item)) {
                    item->setZValue(++zCounter_);
                    itemGroup_->addToGroup(item);
                    mainSelArea_.setReady(true);
                } else {
                    itemGroup_->removeFromGroup(item);
                    if (itemGroup_->isEmpty()) mainSelArea_.setReady(false);
                }
            }
        } else if (event->modifiers() != Qt::ShiftModifier) {
            if (item) {
                if (itemGroup_->isContain(item)) {
                    if (state_ != eGroupItemMoving) {
                        itemGroup_->clearItemGroup();
                        item->setZValue(++zCounter_);
                        itemGroup_->addToGroup(item);
                        mainSelArea_.setReady(true);
                    }

                    state_ = eMouseMoving;
                }

            } else {
                itemGroup_->clearItemGroup();
                mainSelArea_.setReady(false);
                state_ = eMouseMoving;
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
    if ( (event->buttons() & Qt::LeftButton)) {
        if (itemGroup_->isUnderMouse()) {
            state_ = eGroupItemMoving;
        }
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
//    itemGroup_->clearItemGroup();

    auto selItems = selectedItems();

//    for (auto& it : selItems) {
//        itemGroup_->addToGroup(it);
//    }
    LOG_DEBUG(logger, "Selected Items: ", selItems.size());
    LOG_DEBUG(logger, "Group size: ", itemGroup_->childItems().size(), ". Empty: ", itemGroup_->isEmpty());

//    if (itemGroup_->isEmpty()) {
//        mainSelArea_.setReady(false);
//    } else {
//        mainSelArea_.setReady(true);
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
    LOG_WARNING(logger, "!");
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

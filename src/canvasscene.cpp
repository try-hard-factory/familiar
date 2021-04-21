#include "debug_macros.h"
#include "canvasscene.h"
#include "moveitem.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QKeyEvent>
#include <QGraphicsItem>
#include <QPen>
#include <QPainter>
#include <QMimeData>

#include "Logger.h"

#include <regex>

extern Logger logger;


//#define MOUSE_MOVE_DEBUG
CanvasScene::CanvasScene(uint64_t& zc, QGraphicsScene *scene) : zCounter_(zc)
{
    (void)scene;
    itemGroup_ = new ItemGroup(zc);
    LOG_DEBUG(logger, "itemGroup_ Adress: ", itemGroup_, ", Z: ", itemGroup_->zValue());
    itemGroup_->setHandlesChildEvents(true);
    addItem(itemGroup_);
    connect(itemGroup_, &ItemGroup::signalMove, this, &CanvasScene::slotMove);
    imgdownloader_ = new ImageDownloader(*this);

}


void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case (Qt::Key_Delete):
        mainSelArea_.setReady(false);
        for (auto it : itemGroup_->childItems()) {
            itemGroup_->removeItemFromGroup(it);
            removeItem(it);
        }
        break;
    case (Qt::Key_Insert):
        if (event->modifiers() & Qt::ShiftModifier) {
            pasteFromClipboard();
        }
        break;
    default:
        QGraphicsScene::keyPressEvent(event);
        break;
    }
}


void CanvasScene::pasteFromClipboard()
{
    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimedata = clipboard->mimeData(QClipboard::Clipboard);

    if (mimedata->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimedata->imageData());
        if (!image.isNull()) {
            handleImageFromClipboard(image);
        } else if (mimedata->hasHtml()) {
            handleHtmlFromClipboard(mimedata->html());
        }
        if (image.isNull()) LOG_DEBUG(logger, "NULL IMAGE!!!!");
        qDebug()<<image.rect();

    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}


void CanvasScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    event->accept();
}

void CanvasScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{

    if (event->mimeData()->hasUrls()) {
        qDebug()<<"dragEnterEvent hasUrls";
        event->acceptProposedAction();
    }

    event->accept();
}

void CanvasScene::addImageToSceneToPosition(QImage&& image, QPointF position)
{
    ++zCounter_;
    MoveItem* item = new MoveItem(image, zCounter_);
    item->setPos(position);
    addItem(item);
}

void CanvasScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (!mimeData) { return; }

    if (mimeData->hasHtml()) {
        qDebug()<<"dropEvent html"<<mimeData->html();
        qDebug()<<"dropEvent url"<<mimeData->urls();
        imgdownloader_->download(mimeData->urls()[0].toString(), event->scenePos());
    } else if (mimeData->hasUrls()) {
        qreal x = 0;
        ++zCounter_;
        foreach (const QUrl &url, event->mimeData()->urls()) {
            QString fileName = url.toLocalFile();
            qDebug() << "Dropped file:" << fileName<<
                        ", formats: "<< event->mimeData()->formats();

            MoveItem* item = new MoveItem(fileName, zCounter_);
            qreal new_x = event->scenePos().x();
            qreal new_y = event->scenePos().y();
            item->setPos({new_x+x, new_y});
            x += item->getRect().width();
            addItem(item);
        }
    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}

std::string stateText(int idx) {
    switch (idx) {
        case 0: return "eMouseMoving";
        case 1: return "eMouseSelection";
        case 2: return "eGroupItemMoving";
    }
    return "undefined";
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
//    QGraphicsScene::mousePressEvent(event);
//    return;
//    itemGroup_->printChilds();
    auto item = getFirstItemUnderCursor(event->scenePos());
    if (item && item->type() == ItemGroup::eBorderDot) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

//    LOG_DEBUG(logger, "mousePressEvent Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {


        } else if (event->modifiers() != Qt::ShiftModifier) {
//            LOG_WARNING(logger,"HUI ", __LINE__);
            if (item) {
//                LOG_WARNING(logger,"HUI ", __LINE__);
                if (!itemGroup_->isContain(item)) {
                    itemGroup_->clearItemGroup();
                    itemGroup_->addItemToGroup(item);
                    itemGroup_->incZ();
                    mainSelArea_.setReady(true);

                    LOG_DEBUG(logger, "DEBUG MESSAGE1 state: ", stateText(state_));
                }
                state_ = eMouseSelection;
//                LOG_WARNING(logger,"HUI ", __LINE__);
            } else {
//                LOG_WARNING(logger,"HUI ", __LINE__);
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
                    itemGroup_->addItemToGroup(item);
                    itemGroup_->incZ();
                    mainSelArea_.setReady(true);
                } else {
                    itemGroup_->removeItemFromGroup(item);
                    if (itemGroup_->isEmpty()) mainSelArea_.setReady(false);
                }
            }
        } else if (event->modifiers() != Qt::ShiftModifier) {
            if (item) {
                if (itemGroup_->isContain(item)) {
                    LOG_DEBUG(logger, "DEBUG MESSAGE1 state: ", stateText(state_));
                    if (state_ != eGroupItemMoving && state_ != eMouseSelection) {
                        LOG_DEBUG(logger, "DEBUG MESSAGE1 ", __LINE__);
                        itemGroup_->clearItemGroup();
                        item->setZValue(++zCounter_);
                        itemGroup_->addItemToGroup(item);
                        itemGroup_->incZ();
                        mainSelArea_.setReady(true);
                    }

                    state_ = eMouseMoving;
                }
                LOG_DEBUG(logger, "DEBUG MESSAGE1 state: ", stateText(state_));
            } else {
                LOG_DEBUG(logger, "DEBUG MESSAGE1 ", __LINE__);
                if (state_ == eMouseSelection) {
                    LOG_DEBUG(logger, "DEBUG MESSAGE1 ", __LINE__);
                    itemGroup_->clearItemGroup();
                    auto selItems = selectedItems();

                    for (auto& it : selItems) {
                        itemGroup_->addItemToGroup(it);
        //                it->setSelected(false);
                        LOG_DEBUG(logger, "Selected address: ", it);
                    }
                    itemGroup_->incZ();
                    if (itemGroup_->isEmpty()) {
                        mainSelArea_.setReady(false);
                    } else {
                        mainSelArea_.setReady(true);
                    }
                } else {
                    LOG_DEBUG(logger, "DEBUG MESSAGE1 ", __LINE__);
                    itemGroup_->clearItemGroup();
                    mainSelArea_.setReady(false);
                }
                state_ = eMouseMoving;
            }
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}



void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    LOG_DEBUG(logger, "Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")", " State: ", state_);
#endif

    if ( (event->buttons() & Qt::LeftButton)) {
        if (itemGroup_->isUnderMouse() && state_ != eMouseSelection) {
            state_ = eGroupItemMoving;
        }

        if (state_ == eMouseSelection) {

            /*
             *  This part of realtime selection.
             *
             */
////            if (!itemGroup_->isUnderMouse()) {itemGroup_->setSelected(false); }
//            itemGroup_->clearItemGroup();
//            auto selItems = selectedItems();

//            for (auto& it : selItems) {
//                itemGroup_->addItem(it);
////                it->setSelected(false);
//                LOG_DEBUG(logger, "Selected address: ", it);
//            }
//            itemGroup_->incZ();
//            LOG_DEBUG(logger, "itemGroup_ address: ", itemGroup_);
//            LOG_DEBUG(logger, "Selected Items: ", selItems.size());
//            LOG_DEBUG(logger, "Group size: ", itemGroup_->childItems().size(), ". Empty: ", itemGroup_->isEmpty());
//                if (itemGroup_->isEmpty()) {
//                    mainSelArea_.setReady(false);
//                } else {
//                    mainSelArea_.setReady(true);
//                }
        }
    }

    QGraphicsScene::mouseMoveEvent(event);
}

QGraphicsItem* CanvasScene::getFirstItemUnderCursor(const QPointF& p)
{
    auto childs = this->items(Qt::DescendingOrder);

    for (auto& it : childs) {
        if (it != itemGroup_ && it->sceneBoundingRect().contains(p)) {  return it; }
    }

    return nullptr;
}


void CanvasScene::deselectItems()
{
    foreach (QGraphicsItem *item, selectedItems()) {
        item->setSelected(false);
    }
    selectedItems().clear();
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
//        itemGroup_->addItem(it);
//    }
    LOG_WARNING(logger, "Selected Items: ", selItems.size());
//    LOG_DEBUG(logger, "Group size: ", itemGroup_->childItems().size(), ". Empty: ", itemGroup_->isEmpty());

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
    painter->save();
    int wsize = 2;
    painter->setPen( QPen(Qt::red, wsize) );
    auto r = itemGroup_->sceneBoundingRect();
    painter->drawRect(r);

//    auto childs = itemGroup_->childItems();
//    for (auto& it : childs) {
//        if (it->type() == ItemGroup::eBorderDot) continue;
//        painter->drawRect(it->sceneBoundingRect());
//    }
//    painter->setPen( QPen(Qt::black, 2) );
//    auto r = itemGroup_->sceneBoundingRect();
//    painter->drawRect(r);
//    painter->setPen( QPen(Qt::black, 10) );
//    painter->drawPoint(r.x(), r.y());
//    painter->drawPoint(r.x()+r.width(), r.y());
//    painter->drawPoint(r.x()+r.width(), r.y()+r.height());
//    painter->drawPoint(r.x(), r.y() + r.height());
    painter->restore();
}


void CanvasScene::handleImageFromClipboard(const QImage& image)
{
    ++zCounter_;
    MoveItem* item = new MoveItem(image, zCounter_);
    item->setPos(100, 100);
    addItem(item);
}


void CanvasScene::handleHtmlFromClipboard(const QString& html)
{
    std::regex r("<img[^>]*src=['|\"](.*?)['|\"].*?>");
    std::smatch results;
    auto str = html.toStdString();
    std::sregex_iterator it(std::begin(str), std::end(str), r);
    std::sregex_iterator end;

    if (it != end) {
        imgdownloader_->download(QString::fromStdString((*it)[1].str()), {0,0});
    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}

//ItemGroup
void CanvasScene::slotMove(QGraphicsItem *signalOwner, qreal dx, qreal dy)
{
    foreach (QGraphicsItem *item, selectedItems()) {
        if(item != signalOwner) {
            item->moveBy(dx,dy);
        }
    }
}

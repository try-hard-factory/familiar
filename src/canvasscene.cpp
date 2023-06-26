#include "canvasscene.h"
#include "debug_macros.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <QApplication>
#include <QClipboard>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPen>

#include "Logger.h"

#include "project_settings.h"
#include <regex>

extern Logger logger;

#define MOUSE_MOVE_DEBUG
CanvasScene::CanvasScene(MainWindow& mw, uint64_t& zc, QGraphicsScene* scene)
    : mainwindow_(mw)
    , zCounter_(zc)
{
    (void) scene;
    itemGroup_ = new ItemGroup(zc);
    LOG_DEBUG(logger, "itemGroup_ Adress: ", itemGroup_, ", Z: ", itemGroup_->zValue());
    //    itemGroup_->setFiltersChildEvents(true);

    itemGroup_->setPos({0, 0});
    addItem(itemGroup_);

    connect(itemGroup_, &ItemGroup::signalMove, this, &CanvasScene::slotMove);
    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &CanvasScene::settingsChangedSlot);
    settingsChangedSlot();

    imgdownloader_ = new ImageDownloader(*this);

    connect(QApplication::clipboard(),
            &QClipboard::dataChanged,
            this,
            &CanvasScene::clipboardChanged);
}

CanvasScene::~CanvasScene()
{
    delete projectSettings_;
}

void CanvasScene::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case (Qt::Key_Delete):
        mainSelArea_.setReady(false);
        for (auto it : itemGroup_->childItems()) {
            itemGroup_->removeItemFromGroup(it);
            removeItem(it);
        }
        projectSettings_->modified(true);
        break;
    case (Qt::Key_Insert):
        if (event->modifiers() & Qt::ShiftModifier) {
            pasteFromClipboard();
        }
        break;
    case (Qt::Key_V):
        if (event->modifiers() & Qt::ControlModifier) {
            pasteFromClipboard();
        }
        break;
    case (Qt::Key_C):
        if (event->modifiers() & Qt::ControlModifier) {
            copyToClipboard();
        }
        break;
    default:
        QGraphicsScene::keyPressEvent(event);
        break;
    }
}

void CanvasScene::pasteFromClipboard()
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimedata = clipboard->mimeData(QClipboard::Clipboard);

    if (!mainwindow_.clipboardItems().isEmpty()) {
        pasteFromTemp();
    } else if (mimedata->hasUrls()) {
        itemGroup_->clearItemGroup();
        qreal x = 0;
        foreach (const QUrl& url, mimedata->urls()) {
            if (url.isLocalFile()) {
                QString fileName(url.toLocalFile());
                LOG_DEBUG(logger, "Dropped file: ", fileName.toStdString());
                qDebug() << "formats: " << mimedata->formats();

                MoveItem* item = new MoveItem(fileName, zCounter_);
                item->setPos({lastClickedPoint_.x() + x, lastClickedPoint_.y()});
                x += item->getRect().width();
                addItem(item);
                itemGroup_->addItemToGroup(item);
            } else {
                QString urlstr = url.url();
                LOG_DEBUG(logger, "Download.Dropped file: ", urlstr.toStdString());
                imgdownloader_->download(urlstr, lastClickedPoint_);
                projectSettings_->modified(true);
            }
        }
        itemGroup_->incZ();
        mainSelArea_.setReady(true);
        projectSettings_->modified(true);
    } else if (mimedata->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimedata->imageData());
        if (!image.isNull()) {
            handleImageFromClipboard(image);
            projectSettings_->modified(true);
        } else if (mimedata->hasHtml()) {
            handleHtmlFromClipboard(mimedata->html());
            projectSettings_->modified(true);
        }
        if (image.isNull())
            LOG_DEBUG(logger, "NULL IMAGE!!!!");
        qDebug() << "image rect " << image.rect();

    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}

void CanvasScene::pasteFromTemp()
{
    itemGroup_->clearItemGroup();
    for (auto& item : mainwindow_.clipboardItems()) {
        auto widget = qgraphicsitem_cast<MoveItem*>(item);
        MoveItem* tmpitem = new MoveItem(widget->qimage_ptr(), widget->zcounter());
        tmpitem->setPos(widget->pos());

        qDebug() << tmpitem->pos();
        addItem(tmpitem);
        itemGroup_->addItemToGroup(tmpitem);
    }
    itemGroup_->incZ();
    mainSelArea_.setReady(true);
    projectSettings_->modified(true);
}

void CanvasScene::copyToClipboard()
{
    if (itemGroup_->isEmpty())
        return;
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setImage(itemGroup_->mergedImages());
    mainwindow_.clipboardItems(itemGroup_->cloneItems());
}

void CanvasScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    event->accept();
}

void CanvasScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        qDebug() << "dragEnterEvent hasUrls";
        event->acceptProposedAction();
    }

    event->accept();
}

void CanvasScene::addImageToSceneToPosition(QImage&& image, QPointF position)
{
    ++zCounter_;
    QImage* img = new QImage(image);
    MoveItem* item = new MoveItem(img, zCounter_);
    item->setPos(position);
    addItem(item);
}

QByteArray CanvasScene::fml_payload()
{
    QByteArray arr;
    QDataStream ds(&arr, QIODevice::ReadWrite);

    qint16 count = objectsCount();
    ds << count;
    auto items = this->items();
    for (auto& it : items) {
        // \TODO: type.h header with all types (image, textline, multitextline)
        if (it->type() != 3)
            continue;
        auto widget = qgraphicsitem_cast<MoveItem*>(it);

        ds << widget->scenePos() << (qint32) widget->height() << (qint32) widget->width()
           << widget->boundingRect() << (quint16) widget->format();

        QByteArray compressed = qCompress(widget->qimage().constBits(),
                                          widget->qimage().sizeInBytes(),
                                          7);

        qDebug() << widget->qimage().sizeInBytes() << " " << compressed.size() << " "
                 << widget->qimage().bitPlaneCount();

        ds << (quint64) compressed.size();
        qDebug() << "fml_payload::save sizeInBytes: " << widget->qimage().sizeInBytes();
        ds.writeRawData((const char*) compressed.data(), compressed.size());
    }

    return arr;
}

void CanvasScene::setProjectSettings(project_settings* ps)
{
    projectSettings_ = ps;
}

void CanvasScene::cleanupWorkplace()
{
    itemGroup_->clearItemGroup();

    auto childs = this->items(Qt::DescendingOrder);

    for (auto& it : childs) {
        if (it == itemGroup_)
            continue;
        removeItem(it);
        //        delete it;
    }
}

QString CanvasScene::path()
{
    return projectSettings_->path();
}

void CanvasScene::setPath(const QString& path)
{
    projectSettings_->path(path);
}

QString CanvasScene::projectName()
{
    return projectSettings_->projectName();
}

void CanvasScene::setProjectName(const QString& pn)
{
    projectSettings_->projectName(pn);
}

bool CanvasScene::isModified()
{
    return projectSettings_->modified();
}

void CanvasScene::setModified(bool mod)
{
    projectSettings_->modified(mod);
}

bool CanvasScene::isUntitled()
{
    return projectSettings_->isDefaultProjectName();
}

void CanvasScene::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData) {
        return;
    }

    if (mimeData->hasHtml()) {
        itemGroup_->clearItemGroup();
        qDebug() << "dropEvent html" << mimeData->html();
        qDebug() << "dropEvent url" << mimeData->urls();
        imgdownloader_->download(mimeData->urls()[0].toString(), event->scenePos());
        projectSettings_->modified(true);
    } else if (mimeData->hasUrls()) {
        itemGroup_->clearItemGroup();
        qreal x = 0;
        foreach (const QUrl& url, event->mimeData()->urls()) {
            QString fileName = url.toLocalFile();
            LOG_DEBUG(logger, "Dropped file: ", fileName.toStdString());
            qDebug() << "formats: " << event->mimeData()->formats();

            MoveItem* item = new MoveItem(fileName, zCounter_);
            qreal new_x = event->scenePos().x();
            qreal new_y = event->scenePos().y();
            item->setPos({new_x + x, new_y});
            x += item->getRect().width();
            addItem(item);
            itemGroup_->addItemToGroup(item);
        }
        itemGroup_->incZ();
        mainSelArea_.setReady(true);
        projectSettings_->modified(true);
    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}

std::string stateText(int idx)
{
    switch (idx) {
    case 0:
        return "eMouseMoving";
    case 1:
        return "eMouseSelection";
    case 2:
        return "eGroupItemMoving";
    case 3:
        return "eGroupItemResizing";
    }
    return "undefined";
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    //    LOG_DEBUG(logger, "DEBUG: mouse state: ", stateText(state_), "\n");
    auto item = getFirstItemUnderCursor(event->scenePos());
    if (item && item->type() != ItemGroup::eBorderDot) {
        item->setZValue(++zCounter_);
        qDebug() << "zcounter=" << zCounter_ << ", Z VALUE=" << item->zValue();
        //        LOG_DEBUG(logger, "DEBUG: TYPE ", item->type(), "\n");
    }
    if (item && item->type() == ItemGroup::eBorderDot) {
        state_ = eGroupItemResizing;
        LOG_DEBUG(logger, "DEBUG: CHANGE TO mouse state: ", stateText(state_), "\n");
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    //    LOG_DEBUG(logger, "mousePressEvent Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ShiftModifier) {
        } else if (event->modifiers() != Qt::ShiftModifier) {
            if (item) {
                if (!itemGroup_->isContain(item)) {
                    itemGroup_->clearItemGroup();
                    itemGroup_->addItemToGroup(item);
                    itemGroup_->incZ();
                    mainSelArea_.setReady(true);
                    LOG_DEBUG(logger,
                              "Group size: ",
                              itemGroup_->childItems().size(),
                              ". Empty: ",
                              itemGroup_->isEmpty());
                    //                    LOG_DEBUG(logger, "DEBUG MESSAGE1 state: ", stateText(state_));
                }
                //                state_ = eMouseSelection;
            } else {
                state_ = eMouseSelection;
                origin_ = event->scenePos();
                rubberBand_.setRect(origin_.x(), origin_.y(), 0, 0);
                itemGroup_->clearItemGroup();
                mainSelArea_.setReady(false);
                //                state_ = eMouseSelection;
            }
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    //    LOG_DEBUG(logger, "Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");
    lastClickedPoint_ = event->scenePos();
    LOG_DEBUG(logger, "DEBUG: mouse state: ", stateText(state_), "\n");

    if (state_ == eGroupItemResizing || state_ == eMouseSelection) {
        state_ = eMouseMoving;
        LOG_DEBUG(logger, "DEBUG: CHANGE TO mouse state: ", stateText(state_), "\n");
        itemGroup_->notifyCursorUpdater(event, parentViewScaleFactor_);
        QGraphicsScene::mouseReleaseEvent(event);
        return;
    }

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
                    if (itemGroup_->isEmpty()) {
                        itemGroup_->clearItemGroup();
                        mainSelArea_.setReady(false);
                    }
                }
            }
        } else if (event->modifiers() != Qt::ShiftModifier) {
            if (item) {
                if (itemGroup_->isContain(item) && !itemGroup_->isThisDots(item)) {
                    if (state_ != eGroupItemMoving) {
                        itemGroup_->clearItemGroup();
                        item->setZValue(++zCounter_);
                        itemGroup_->addItemToGroup(item);
                        itemGroup_->incZ();
                        mainSelArea_.setReady(true);
                    }

                    state_ = eMouseMoving;
                    LOG_DEBUG(logger, "DEBUG: CHANGE TO mouse state: ", stateText(state_), "\n");
                }
            } else {
                itemGroup_->clearItemGroup();
                mainSelArea_.setReady(false);
                state_ = eMouseMoving;
                LOG_DEBUG(logger, "DEBUG: CHANGE TO mouse state: ", stateText(state_), "\n");
            }
        }
    }

    itemGroup_->notifyCursorUpdater(event, parentViewScaleFactor_);
    QGraphicsScene::mouseReleaseEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
#ifdef MOUSE_MOVE_DEBUG
    //    qDebug()<<'\n';
    //    qDebug()<<"CanvasScene:: pos: "<<event->pos()<<", scenePos: "<<event->scenePos();
    //    qDebug()<<"CanvasScene:: itemGroup_ pos: "<<itemGroup_->pos()<<", scenePos: "<<itemGroup_->scenePos();
    //    qDebug()<<'\n';
#endif
    //    LOG_DEBUG(logger, "DEBUG: mouse state: ", stateText(state_), ". Pos = ", event->scenePos().x(), ",",event->scenePos().y(), "\n");
    if ((event->buttons() & Qt::LeftButton)) {
        if (state_ == eMouseSelection) {
            QRectF tmp(origin_, event->scenePos());
            rubberBand_ = tmp;
            rubberBand_ = rubberBand_.normalized();

            //            qDebug()<<rubberBand_<<" "<<items(rubberBand_).size();
            itemGroup_->clearItemGroup();
            auto selItems = items(rubberBand_);
            for (auto& it : selItems) {
                if (it->type() == 3)
                    itemGroup_->addItemToGroup(it);
                //            it->setSelected(false);
            }
            itemGroup_->incZ();
            //            LOG_DEBUG(logger, "itemGroup_ address: ", itemGroup_);
            //            LOG_DEBUG(logger, "Selected Items: ", selItems.size());
            //            LOG_DEBUG(logger, "Group size: ", itemGroup_->childItems().size(), ". Empty: ", itemGroup_->isEmpty());
            if (itemGroup_->isEmpty()) {
                mainSelArea_.setReady(false);
            } else {
                mainSelArea_.setReady(true);
            }
        } else if (state_ == eGroupItemResizing || state_ == eGroupItemMoving) {
            projectSettings_->modified(true);
        } else if (itemGroup_->isUnderMouse() && state_ != eGroupItemResizing) {
            state_ = eGroupItemMoving;
            //            LOG_DEBUG(logger, "DEBUG: CHANGE TO mouse state: ", stateText(state_), "\n");
        }
    }

    itemGroup_->notifyCursorUpdater(event, parentViewScaleFactor_);
    QGraphicsScene::mouseMoveEvent(event);
}

QGraphicsItem* CanvasScene::getFirstItemUnderCursor(const QPointF& p)
{
    auto childs = this->items(Qt::DescendingOrder);

    for (auto& it : childs) {
        if (it == itemGroup_)
            continue;
        if (it->type() == 65537) { // borderdot control
            // NOTE: Maybe I must reimplement QGraphicsItem::contains,
            //       but I don't know how yet.
            auto point = p - it->scenePos();
            auto len = std::sqrt(std::pow(point.x(), 2) + std::pow(point.y(), 2));
            int x = 4;
            if ((len * parentViewScaleFactor_ - x) < 0)
                return it;
        } else {
            if (it->sceneBoundingRect().contains(p)) {
                return it;
            }
        }
    }

    return nullptr;
}

void CanvasScene::deselectItems()
{
    foreach (QGraphicsItem* item, selectedItems()) {
        item->setSelected(false);
    }
    selectedItems().clear();
}

bool CanvasScene::isAnySelectedUnderCursor() const
{
    auto selItems = selectedItems();
    //    LOG_DEBUG(logger, "Selected Items: ", selItems.size());

    for (auto& it : selItems) {
        if (it->isUnderMouse())
            return true;
    }

    return false;
}

void CanvasScene::onSelectionChanged()
{
    auto selItems = selectedItems();
    (void) selItems;
}

void CanvasScene::drawForeground(QPainter* painter, const QRectF& rect)
{
#ifdef GRID_DEBUG
    painter->setPen(QPen(Qt::green, 3));
    painter->drawEllipse(itemGroup_->pos(), 6, 6);
    painter->setPen(QPen(Qt::red, 3));
    painter->drawEllipse(itemGroup_->scenePos(), 10, 10);
    painter->setPen(QPen(Qt::white, 3));
    painter->drawEllipse({0, 0}, 2, 2);
    painter->setPen(QPen(Qt::black, 1));
    int begin = -3000;
    while (begin != 3000) {
        painter->drawLine(-99999, begin, 9999, begin);
        painter->drawLine(begin, -99999, begin, 99999);
        begin += 100;
    }
#endif
    if (!mainSelArea_.isReady())
        return;
    painter->save();
    qreal wsize = 2;
    QPen outline_pen{selectionColor_, wsize};
    outline_pen.setCosmetic(true);
    painter->setPen(outline_pen);
    auto r = itemGroup_->sceneBoundingRect();
    painter->drawRect(r);
    painter->restore();
}

void CanvasScene::handleImageFromClipboard(const QImage& image)
{
    ++zCounter_;
    QImage* img = new QImage(image);
    MoveItem* item = new MoveItem(img, zCounter_);
    item->setPos(lastClickedPoint_);
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
        imgdownloader_->download(QString::fromStdString((*it)[1].str()), {0, 0});
    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}
// void CanvasScene::handleHtmlFromClipboard(const QString& html)
// {
//     QRegularExpression re("<img[^>]*src=['|\"](.*?)['|\"].*?>");
//     QRegularExpressionMatchIterator it = re.globalMatch(html);

//     if (it.hasNext()) {
//         QRegularExpressionMatch match = it.next();
//         QString imageUrl = match.captured(1);
//         imgdownloader_->download(imageUrl, {0, 0});
//     } else {
//         LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
//     }
// }

//ItemGroup
void CanvasScene::slotMove(QGraphicsItem* signalOwner, qreal dx, qreal dy)
{
    for (QGraphicsItem* item : selectedItems()) {
        if (item != signalOwner) {
            item->moveBy(dx, dy);
        }
    }
}

void CanvasScene::settingsChangedSlot()
{
    auto settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    selectionColor_ = colorPreset[EPresetsColorIdx::kSelectionColor];
}

void CanvasScene::clipboardChanged()
{
    mainwindow_.clearClipboardItems();
}

qint16 CanvasScene::objectsCount() const
{
    // \TODO: type.h header with all types (image, textline, multitextline)
    const int targetObjectType = 3;
    return std::count_if(items().begin(),
                         items().end(),
                         [targetObjectType](const QGraphicsItem* item) {
                             return item->type() == targetObjectType;
                         });
}
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
#include "selector.h"
#include <regex>

#include <QUndoStack>

extern Logger logger;

#define MOUSE_MOVE_DEBUG
CanvasScene::CanvasScene(MainWindow& mw,
                         uint64_t& zc,
                         QUndoStack* undoStack,
                         QGraphicsScene* scene)
    : undo_stack_(undoStack)
    , mainwindow_(mw)
    , zCounter_(zc)
{
    connect(this,
            &CanvasScene::selectionChanged,
            this,
            &CanvasScene::on_selection_changed);
    connect(this, &CanvasScene::changed, this, &CanvasScene::on_change);
    (void) scene;
    clear();
    clear_ongoing = false;

    // itemGroup_ = new ItemGroup(zc);
    // LOG_DEBUG(logger,
    //           "itemGroup_ Adress: ",
    //           itemGroup_,
    //           ", Z: ",
    //           itemGroup_->zValue());
    // //    itemGroup_->setFiltersChildEvents(true);

    // itemGroup_->setPos({0, 0});
    // addItem(itemGroup_);

    // connect(itemGroup_, &ItemGroup::signalMove, this, &CanvasScene::slotMove);
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

void CanvasScene::clear()
{
    clear_ongoing = true;
    QGraphicsScene::clear();
    internal_clipboard.clear();
    rubberband_item_ = new RubberbandItem();
    multiselect_item_ = new MultiSelectItem();
    clear_ongoing = false;
}

void CanvasScene::addItem(QGraphicsItem* item)
{
    qDebug() << "Adding item" << item;
    QGraphicsScene::addItem(item);
}


void CanvasScene::removeItem(QGraphicsItem* item)
{
    qDebug() << "Removing item" << item;
    QGraphicsScene::removeItem(item);
}

void CanvasScene::cancel_active_modes()
{
    cancel_crop_mode();
    end_rubberband_mode();
}

void CanvasScene::end_rubberband_mode()
{
    if (rubberband_item_) {
        qDebug() << "End rubberband mode";
        removeItem(rubberband_item_);
    }
    rubberband_active = false;
}

void CanvasScene::cancel_crop_mode()
{
    if (crop_item) {
        qDebug() << "End crop mode";
        crop_item->exit_crop_mode(false);
    }
}

void CanvasScene::copy_selection_to_internal_clipboard()
{
    internal_clipboard.clear();
    for (QGraphicsItem* item : selectedItems(true)) {
        internal_clipboard.append(dynamic_cast<IBaseItem*>(item));
    }
}

void CanvasScene::paste_from_internal_clipboard(QPointF position)
{
    QList<IBaseItem*> copies;
    for (IBaseItem* item : internal_clipboard) {
        IBaseItem* copy = item->create_copy();
        copies.append(copy);
    }
    // TODOLATER: use undo stack
    // undo_stack_->push(new InsertItemsCommand(this, copies, position));
}

void CanvasScene::raise_to_top()
{
    cancel_active_modes();
    QList<QGraphicsItem*> items = selectedItems(true);
    std::vector<double> z_values;
    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(z_values),
                   [](const auto& i) { return i->zValue(); });
    double min_z_value = *std::min_element(z_values.begin(), z_values.end());
    double delta = max_z + Z_STEP - min_z_value;
    qDebug() << "Raise to top, delta: " << delta;
    for (auto& item : items) {
        item->setZValue(item->zValue() + delta);
    }
}

void CanvasScene::lower_to_bottom()
{
    cancel_active_modes();
    QList<QGraphicsItem*> items = selectedItems(true);
    std::vector<double> z_values;
    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(z_values),
                   [](const auto& i) { return i->zValue(); });
    double max_z_value = *std::max_element(z_values.begin(), z_values.end());
    double delta = min_z - Z_STEP - max_z_value;
    qDebug() << "Lower to bottom, delta: " << delta;
    for (auto& item : items) {
        item->setZValue(item->zValue() + delta);
    }
}


void CanvasScene::normalize_width_or_height(const QString& mode)
{
    cancel_active_modes();
    QList<qreal> values;
    QList<QGraphicsItem*> items = selectedItems(true);
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        values.append(mode == "width" ? rect.width() : rect.height());
    }
    if (values.size() < 2)
        return;
    qreal avg = std::accumulate(values.constBegin(), values.constEnd(), 0.0)
                / values.size();
    qDebug() << "Calculated average" << mode << avg;

    QList<qreal> scaleFactors;
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        scaleFactors.append(avg
                            / (mode == "width" ? rect.width() : rect.height()));
    }
    // TODOLATER: use undo stack
    // undo_stack_->push(new NormalizeItemsCommand(items, scaleFactors));
}

void CanvasScene::normalize_height()
{
    normalize_width_or_height("height");
}
void CanvasScene::normalize_width()
{
    normalize_width_or_height("width");
}

void CanvasScene::normalize_size()
{
    cancel_active_modes();
    QList<qreal> sizes;
    QList<QGraphicsItem*> items = selectedItems(true);
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        sizes.append(rect.width() * rect.height());
    }
    if (sizes.size() < 2)
        return;
    qreal avg = std::accumulate(sizes.constBegin(), sizes.constEnd(), 0.0)
                / sizes.size();
    qDebug() << "Calculated average size" << avg;

    QList<qreal> scaleFactors;
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        scaleFactors.append(std::sqrt(avg / (rect.width() * rect.height())));
    }
    // TODOLATER: use undo stack
    // undoStack->push(new NormalizeItemsCommand(items, scaleFactors));
}

void CanvasScene::arrange(bool vertical)
{
    // TODOLATER:
    qDebug() << "CanvasScene::arrange: not implemented";
}

void CanvasScene::arrange_optimal()
{
    // TODOLATER:
    qDebug() << "CanvasScene::arrange_optimal: not implemented";
}

void CanvasScene::flip_items(bool vertical)
{
    cancel_active_modes();
    // TODOLATER:
    // undoStack->push(new FlipItemsCommand(selectedItems(userOnly), getSelectionCenter(), vertical));
}

void CanvasScene::crop_items()
{
    if (crop_item)
        return;
    if (has_single_image_selection()) {
        IBaseItem* item = (IBaseItem*) selectedItems(true).first();
        if (item->is_croppable())
            item->enter_crop_mode();
    }
}

QColor CanvasScene::sample_color_at(QPointF position)
{
    // TODOLATER:
    qDebug() << "CanvasScene::sample_color_at: not implemented";
    return QColor();
}

void CanvasScene::select_all_items()
{
    cancel_active_modes();
    QPainterPath path;
    path.addRect(itemsBoundingRect());
    setSelectionArea(path);
}

void CanvasScene::deselect_all_items()
{
    cancel_active_modes();
    clearSelection();
}

bool CanvasScene::has_selection()
{
    return !selectedItems(true).isEmpty();
}

bool CanvasScene::has_single_selection()
{
    return selectedItems(true).size() == 1;
}

bool CanvasScene::has_multi_selection()
{
    return selectedItems(true).size() > 1;
}

bool CanvasScene::has_single_image_selection()
{
    if (has_single_selection()) {
        return selectedItems(true).first()->type() == 777;
    }
    return false;
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        event_start = event->scenePos();
        auto* item_at_pos = itemAt(event_start, views().first()->transform());

        if (edit_item) {
            if (item_at_pos != edit_item)
                edit_item->exit_edit_mode();
            else {
                QGraphicsScene::mousePressEvent(event);
                return;
            }
        }

        if (crop_item) {
            if (item_at_pos != crop_item)
                cancel_crop_mode();
            else {
                QGraphicsScene::mousePressEvent(event);
                return;
            }
        }

        if (item_at_pos) {
            move_active = true;
        } else if (!items().isEmpty()) {
            rubberband_active = true;
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    cancel_active_modes();

    auto* item = itemAt(event->scenePos(), views().first()->transform());
    if (item) {
        if (!item->isSelected()) {
            item->setSelected(true);
        }
        if (((IBaseItem*) item)->is_editable()) {
            ((TextItem*) item)->enter_edit_mode();
            // TODOLATER:
            QGraphicsScene::mousePressEvent(event);
        } else {
            CanvasView* view = static_cast<CanvasView*>(views().first());
            view->fitRect(itemsBoundingRect(false, QList<QGraphicsItem*>{item}),
                          item);
        }
        return;
    }

    QGraphicsScene::mouseDoubleClickEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (rubberband_active) {
        if (!rubberband_item_->scene()) {
            qDebug() << "Activating rubberband selection";
            addItem(rubberband_item_);
            rubberband_item_->bring_to_front();
        }
        rubberband_item_->fit(event_start, event->scenePos());
        setSelectionArea(rubberband_item_->shape());
        CanvasView* view = static_cast<CanvasView*>(views().first());
        view->resetPreviousTransform();
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (rubberband_active) {
        end_rubberband_mode();
    }
    if (move_active && has_selection() && !multiselect_item_->is_action_active()
        && !((IBaseItem*) selectedItems(true).first())->is_action_active()) {
        auto delta = event->scenePos() - event_start;
        if (!delta.isNull()) {
            // TODOLATER:
            // undo_stack_->push(new MoveItemsByCommand(selectedItems(), delta, true));
        }
    }

    rubberband_active = false;
    move_active = false;
    QGraphicsScene::mouseReleaseEvent(event);
}

QList<QGraphicsItem*> CanvasScene::selectedItems(bool userOnly) const
{
    QList<QGraphicsItem*> items = QGraphicsScene::selectedItems();
    if (userOnly) {
        QList<QGraphicsItem*> userItems;
        for (QGraphicsItem* item : items) {
            if (item->data(0).isValid() && itemAddByUser(item->type()))
                userItems.append(item);
        }
        return userItems;
    }
    return items;
}

QList<QGraphicsItem*> CanvasScene::items_by_type(int type)
{
    QList<QGraphicsItem*> itemsl;
    for (QGraphicsItem* item : items()) {
        if (item->type() == type)
            itemsl.append(item);
    }
    return itemsl;
}

QList<QGraphicsItem*> CanvasScene::items_for_save()
{
    Q_ASSERT_X(false, "CanvasScene::items_for_save", "Not implemented");
    return QList<QGraphicsItem*>();
}

void CanvasScene::clear_save_ids()
{
    Q_ASSERT_X(false, "CanvasScene::clear_save_ids", "Not implemented");
}

void CanvasScene::on_view_scale_change()
{
    for (QGraphicsItem* item : selectedItems()) {
        // TODOLATER: Может сделать в IBaseItem виртуальный метод,
        // который я переопределю в SelectableMixin
        if (item->type() == 777) {
            auto* pixmap_item = (PixmapItem*) item;
            pixmap_item->on_view_scale_change();
        } else if (item->type() == 666) {
            auto* text_item = (TextItem*) item;
            text_item->on_view_scale_change();
        }
    }
}

QRectF CanvasScene::itemsBoundingRect(bool selectionOnly,
                                      QList<QGraphicsItem*> items_in) const
{
    auto filterUserItems =
        [this](const QList<QGraphicsItem*>& itemList) -> QList<QGraphicsItem*> {
        QList<QGraphicsItem*> userItems;
        for (QGraphicsItem* item : itemList) {
            if (item->data(0).isValid() && this->itemAddByUser(item->type()))
                userItems.append(item);
        }
        return userItems;
    };

    QList<QGraphicsItem*> base;

    if (selectionOnly) {
        base = filterUserItems(selectedItems());
    } else if (!items_in.isEmpty()) {
        base = items_in;
    } else {
        base = filterUserItems(items());
    }

    if (base.isEmpty()) {
        return QRectF(0, 0, 0, 0);
    }

    Q_ASSERT_X(false, "CanvasScene::itemsBoundingRect", "Not implemented");
}

QPointF CanvasScene::get_selection_center()
{
    auto rect = itemsBoundingRect(true);
    return (rect.topLeft() + rect.bottomRight()) / 2;
}

void CanvasScene::on_selection_changed()
{
    if (clear_ongoing) {
        return;
    }

    if (has_multi_selection()) {
        multiselect_item_->fit_selection_area(itemsBoundingRect(true));
    }

    if (has_multi_selection() && !multiselect_item_->scene()) {
        addItem(multiselect_item_);
        multiselect_item_->bring_to_front();
    }

    if (!has_multi_selection() && multiselect_item_->scene()) {
        removeItem(multiselect_item_);
    }
}

void CanvasScene::on_change()
{
    if (clear_ongoing) {
        return;
    }

    if (multiselect_item_->scene() && !multiselect_item_->is_action_active()) {
        multiselect_item_->fit_selection_area(itemsBoundingRect(true));
    }
}

bool CanvasScene::itemAddByUser(int type) const
{
    return (type == 666) || (type == 777);
}


// OLD CODE


void CanvasScene::keyPressEvent(QKeyEvent* event)
{
    // switch (event->key()) {
    // case (Qt::Key_Delete):
    //     mainSelArea_.setReady(false);
    //     for (auto it : itemGroup_->childItems()) {
    //         itemGroup_->removeItemFromGroup(it);
    //         removeItem(it);
    //     }
    //     projectSettings_->modified(true);
    //     break;
    // case (Qt::Key_Insert):
    //     if (event->modifiers() & Qt::ShiftModifier) {
    //         pasteFromClipboard();
    //     }
    //     break;
    // case (Qt::Key_V):
    //     if (event->modifiers() & Qt::ControlModifier) {
    //         pasteFromClipboard();
    //     }
    //     break;
    // case (Qt::Key_C):
    //     if (event->modifiers() & Qt::ControlModifier) {
    //         copyToClipboard();
    //     }
    //     break;
    // default:
    //     QGraphicsScene::keyPressEvent(event);
    //     break;
    // }
}

void CanvasScene::pasteFromClipboard()
{
    // const QClipboard* clipboard = QApplication::clipboard();
    // const QMimeData* mimedata = clipboard->mimeData(QClipboard::Clipboard);

    // if (!mainwindow_.clipboardItems().isEmpty()) {
    //     pasteFromTemp();
    // } else if (mimedata->hasUrls()) {
    //     itemGroup_->clearItemGroup();
    //     qreal x = 0;
    //     foreach (const QUrl& url, mimedata->urls()) {
    //         if (url.isLocalFile()) {
    //             QString fileName(url.toLocalFile());
    //             LOG_DEBUG(logger, "Dropped file: ", fileName.toStdString());
    //             qDebug() << "formats: " << mimedata->formats();

    //             MoveItem* item = new MoveItem(fileName, zCounter_);
    //             item->setPos({lastClickedPoint_.x() + x, lastClickedPoint_.y()});
    //             x += item->getRect().width();
    //             addItem(item);
    //             itemGroup_->addItemToGroup(item);
    //         } else {
    //             QString urlstr = url.url();
    //             LOG_DEBUG(logger,
    //                       "Download.Dropped file: ",
    //                       urlstr.toStdString());
    //             imgdownloader_->download(urlstr, lastClickedPoint_);
    //             projectSettings_->modified(true);
    //         }
    //     }
    //     itemGroup_->incZ();
    //     mainSelArea_.setReady(true);
    //     projectSettings_->modified(true);
    // } else if (mimedata->hasImage()) {
    //     QImage image = qvariant_cast<QImage>(mimedata->imageData());
    //     if (!image.isNull()) {
    //         handleImageFromClipboard(image);
    //         projectSettings_->modified(true);
    //     } else if (mimedata->hasHtml()) {
    //         handleHtmlFromClipboard(mimedata->html());
    //         projectSettings_->modified(true);
    //     }
    //     if (image.isNull())
    //         LOG_DEBUG(logger, "NULL IMAGE!!!!");
    //     qDebug() << "image rect " << image.rect();

    // } else {
    //     LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    // }
}

void CanvasScene::pasteFromTemp()
{
    // itemGroup_->clearItemGroup();
    // for (auto& item : mainwindow_.clipboardItems()) {
    //     auto widget = qgraphicsitem_cast<MoveItem*>(item);
    //     MoveItem* tmpitem = new MoveItem(widget->qimage_ptr(),
    //                                      widget->zcounter());
    //     tmpitem->setPos(widget->pos());

    //     qDebug() << tmpitem->pos();
    //     addItem(tmpitem);
    //     itemGroup_->addItemToGroup(tmpitem);
    // }
    // itemGroup_->incZ();
    // mainSelArea_.setReady(true);
    // projectSettings_->modified(true);
}

void CanvasScene::copyToClipboard()
{
    // if (itemGroup_->isEmpty())
    //     return;
    // QClipboard* clipboard = QApplication::clipboard();
    // clipboard->setImage(itemGroup_->mergedImages());
    // mainwindow_.clipboardItems(itemGroup_->cloneItems());
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
    // ++zCounter_;
    // QImage* img = new QImage(image);
    // MoveItem* item = new MoveItem(img, zCounter_);
    // item->setPos(position);
    // addItem(item);
}

QByteArray CanvasScene::fml_payload()
{
    QByteArray arr;
    // QDataStream ds(&arr, QIODevice::ReadWrite);

    // qint16 count = objectsCount();
    // ds << count;
    // auto items = this->items();
    // for (auto& it : items) {
    //     // \TODO: type.h header with all types (image, textline, multitextline)
    //     if (it->type() != 3)
    //         continue;
    //     auto widget = qgraphicsitem_cast<MoveItem*>(it);

    //     ds << widget->scenePos() << (qint32) widget->height()
    //        << (qint32) widget->width() << widget->boundingRect()
    //        << (quint16) widget->format();

    //     QByteArray compressed = qCompress(widget->qimage().constBits(),
    //                                       widget->qimage().sizeInBytes(),
    //                                       7);

    //     qDebug() << widget->qimage().sizeInBytes() << " " << compressed.size()
    //              << " " << widget->qimage().bitPlaneCount();

    //     ds << (quint64) compressed.size();
    //     qDebug() << "fml_payload::save sizeInBytes: "
    //              << widget->qimage().sizeInBytes();
    //     ds.writeRawData((const char*) compressed.data(), compressed.size());
    // }

    return arr;
}

void CanvasScene::setProjectSettings(project_settings* ps)
{
    projectSettings_ = ps;
}

void CanvasScene::cleanupWorkplace()
{
    // itemGroup_->clearItemGroup();

    // auto childs = this->items(Qt::DescendingOrder);

    // for (auto& it : childs) {
    //     if (it == itemGroup_)
    //         continue;
    //     removeItem(it);
    //     //        delete it;
    // }
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
    // const QMimeData* mimeData = event->mimeData();
    // if (!mimeData) {
    //     return;
    // }

    // if (mimeData->hasHtml()) {
    //     itemGroup_->clearItemGroup();
    //     qDebug() << "dropEvent html" << mimeData->html();
    //     qDebug() << "dropEvent url" << mimeData->urls();
    //     imgdownloader_->download(mimeData->urls()[0].toString(),
    //                              event->scenePos());
    //     projectSettings_->modified(true);
    // } else if (mimeData->hasUrls()) {
    //     itemGroup_->clearItemGroup();
    //     qreal x = 0;
    //     foreach (const QUrl& url, event->mimeData()->urls()) {
    //         QString fileName = url.toLocalFile();
    //         LOG_DEBUG(logger, "Dropped file: ", fileName.toStdString());
    //         qDebug() << "formats: " << event->mimeData()->formats();

    //         MoveItem* item = new MoveItem(fileName, zCounter_);
    //         qreal new_x = event->scenePos().x();
    //         qreal new_y = event->scenePos().y();
    //         item->setPos({new_x + x, new_y});
    //         x += item->getRect().width();
    //         addItem(item);
    //         itemGroup_->addItemToGroup(item);
    //     }
    //     itemGroup_->incZ();
    //     mainSelArea_.setReady(true);
    //     projectSettings_->modified(true);
    // } else {
    //     LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    // }
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

// void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
// {
// //    LOG_DEBUG(logger, "DEBUG: mouse state: ", stateText(state_), "\n");
// auto item = getFirstItemUnderCursor(event->scenePos());
// if (item && item->type() != ItemGroup::eBorderDot) {
//     item->setZValue(++zCounter_);
//     qDebug() << "zcounter=" << zCounter_ << ", Z VALUE=" << item->zValue();
//     //        LOG_DEBUG(logger, "DEBUG: TYPE ", item->type(), "\n");
// }
// if (item && item->type() == ItemGroup::eBorderDot) {
//     state_ = eGroupItemResizing;
//     LOG_DEBUG(logger,
//               "DEBUG: CHANGE TO mouse state: ",
//               stateText(state_),
//               "\n");
//     QGraphicsScene::mousePressEvent(event);
//     return;
// }

// //    LOG_DEBUG(logger, "mousePressEvent Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");

// if (event->button() == Qt::LeftButton) {
//     if (event->modifiers() == Qt::ShiftModifier) {
//     } else if (event->modifiers() != Qt::ShiftModifier) {
//         if (item) {
//             if (!itemGroup_->isContain(item)) {
//                 itemGroup_->clearItemGroup();
//                 itemGroup_->addItemToGroup(item);
//                 itemGroup_->incZ();
//                 mainSelArea_.setReady(true);
//                 LOG_DEBUG(logger,
//                           "Group size: ",
//                           itemGroup_->childItems().size(),
//                           ". Empty: ",
//                           itemGroup_->isEmpty());
//                 //                    LOG_DEBUG(logger, "DEBUG MESSAGE1 state: ", stateText(state_));
//             }
//             //                state_ = eMouseSelection;
//         } else {
//             state_ = eMouseSelection;
//             origin_ = event->scenePos();
//             rubberBand_.setRect(origin_.x(), origin_.y(), 0, 0);
//             itemGroup_->clearItemGroup();
//             mainSelArea_.setReady(false);
//             //                state_ = eMouseSelection;
//         }
//     }
// }
// QGraphicsScene::mousePressEvent(event);
// }

// void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
// {
// //    LOG_DEBUG(logger, "Event->scenePos: (", event->scenePos().x(),";",event->scenePos().y(), ")");
// lastClickedPoint_ = event->scenePos();
// LOG_DEBUG(logger, "DEBUG: mouse state: ", stateText(state_), "\n");

// if (state_ == eGroupItemResizing || state_ == eMouseSelection) {
//     state_ = eMouseMoving;
//     LOG_DEBUG(logger,
//               "DEBUG: CHANGE TO mouse state: ",
//               stateText(state_),
//               "\n");
//     itemGroup_->notifyCursorUpdater(event, parentViewScaleFactor_);
//     QGraphicsScene::mouseReleaseEvent(event);
//     return;
// }

// auto item = getFirstItemUnderCursor(event->scenePos());
// if (event->button() == Qt::LeftButton) {
//     if (event->modifiers() == Qt::ShiftModifier) {
//         if (item) { // add to group
//             if (!itemGroup_->isContain(item)) {
//                 item->setZValue(++zCounter_);
//                 itemGroup_->addItemToGroup(item);
//                 itemGroup_->incZ();
//                 mainSelArea_.setReady(true);
//             } else {
//                 itemGroup_->removeItemFromGroup(item);
//                 if (itemGroup_->isEmpty()) {
//                     itemGroup_->clearItemGroup();
//                     mainSelArea_.setReady(false);
//                 }
//             }
//         }
//     } else if (event->modifiers() != Qt::ShiftModifier) {
//         if (item) {
//             if (itemGroup_->isContain(item)
//                 && !itemGroup_->isThisDots(item)) {
//                 if (state_ != eGroupItemMoving) {
//                     itemGroup_->clearItemGroup();
//                     item->setZValue(++zCounter_);
//                     itemGroup_->addItemToGroup(item);
//                     itemGroup_->incZ();
//                     mainSelArea_.setReady(true);
//                 }

//                 state_ = eMouseMoving;
//                 LOG_DEBUG(logger,
//                           "DEBUG: CHANGE TO mouse state: ",
//                           stateText(state_),
//                           "\n");
//             }
//         } else {
//             itemGroup_->clearItemGroup();
//             mainSelArea_.setReady(false);
//             state_ = eMouseMoving;
//             LOG_DEBUG(logger,
//                       "DEBUG: CHANGE TO mouse state: ",
//                       stateText(state_),
//                       "\n");
//         }
//     }
// }

// itemGroup_->notifyCursorUpdater(event, parentViewScaleFactor_);
// QGraphicsScene::mouseReleaseEvent(event);
// }

// void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
// {
// #ifdef MOUSE_MOVE_DEBUG
//     //    qDebug()<<'\n';
//     //    qDebug()<<"CanvasScene:: pos: "<<event->pos()<<", scenePos: "<<event->scenePos();
//     //    qDebug()<<"CanvasScene:: itemGroup_ pos: "<<itemGroup_->pos()<<", scenePos: "<<itemGroup_->scenePos();
//     //    qDebug()<<'\n';
// #endif
//     //    LOG_DEBUG(logger, "DEBUG: mouse state: ", stateText(state_), ". Pos = ", event->scenePos().x(), ",",event->scenePos().y(), "\n");
//     if ((event->buttons() & Qt::LeftButton)) {
//         if (state_ == eMouseSelection) {
//             QRectF tmp(origin_, event->scenePos());
//             rubberBand_ = tmp;
//             rubberBand_ = rubberBand_.normalized();

//             //            qDebug()<<rubberBand_<<" "<<items(rubberBand_).size();
//             itemGroup_->clearItemGroup();
//             auto selItems = items(rubberBand_);
//             for (auto& it : selItems) {
//                 if (it->type() == 3)
//                     itemGroup_->addItemToGroup(it);
//                 //            it->setSelected(false);
//             }
//             itemGroup_->incZ();
//             //            LOG_DEBUG(logger, "itemGroup_ address: ", itemGroup_);
//             //            LOG_DEBUG(logger, "Selected Items: ", selItems.size());
//             //            LOG_DEBUG(logger, "Group size: ", itemGroup_->childItems().size(), ". Empty: ", itemGroup_->isEmpty());
//             if (itemGroup_->isEmpty()) {
//                 mainSelArea_.setReady(false);
//             } else {
//                 mainSelArea_.setReady(true);
//             }
//         } else if (state_ == eGroupItemResizing || state_ == eGroupItemMoving) {
//             projectSettings_->modified(true);
//         } else if (itemGroup_->isUnderMouse() && state_ != eGroupItemResizing) {
//             state_ = eGroupItemMoving;
//             //            LOG_DEBUG(logger, "DEBUG: CHANGE TO mouse state: ", stateText(state_), "\n");
//         }
//     }

//     itemGroup_->notifyCursorUpdater(event, parentViewScaleFactor_);
//     QGraphicsScene::mouseMoveEvent(event);
// }

QGraphicsItem* CanvasScene::getFirstItemUnderCursor(const QPointF& p)
{
    // auto childs = this->items(Qt::DescendingOrder);

    // for (auto& it : childs) {
    //     if (it == itemGroup_)
    //         continue;
    //     if (it->type() == 65537) { // borderdot control
    //         // NOTE: Maybe I must reimplement QGraphicsItem::contains,
    //         //       but I don't know how yet.
    //         auto point = p - it->scenePos();
    //         auto len = std::sqrt(std::pow(point.x(), 2)
    //                              + std::pow(point.y(), 2));
    //         int x = 4;
    //         if ((len * parentViewScaleFactor_ - x) < 0)
    //             return it;
    //     } else {
    //         if (it->sceneBoundingRect().contains(p)) {
    //             return it;
    //         }
    //     }
    // }

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
    // #ifdef GRID_DEBUG
    //     painter->setPen(QPen(Qt::green, 3));
    //     painter->drawEllipse(itemGroup_->pos(), 6, 6);
    //     painter->setPen(QPen(Qt::red, 3));
    //     painter->drawEllipse(itemGroup_->scenePos(), 10, 10);
    //     painter->setPen(QPen(Qt::white, 3));
    //     painter->drawEllipse({0, 0}, 2, 2);
    //     painter->setPen(QPen(Qt::black, 1));
    //     int begin = -3000;
    //     while (begin != 3000) {
    //         painter->drawLine(-99999, begin, 9999, begin);
    //         painter->drawLine(begin, -99999, begin, 99999);
    //         begin += 100;
    //     }
    // #endif
    //     if (!mainSelArea_.isReady())
    //         return;
    //     painter->save();
    //     qreal wsize = 2;
    //     QPen outline_pen{selectionColor_, wsize};
    //     outline_pen.setCosmetic(true);
    //     painter->setPen(outline_pen);
    //     auto r = itemGroup_->sceneBoundingRect();
    //     painter->drawRect(r);
    //     painter->restore();
}

void CanvasScene::handleImageFromClipboard(const QImage& image)
{
    // ++zCounter_;
    // QImage* img = new QImage(image);
    // MoveItem* item = new MoveItem(img, zCounter_);
    // item->setPos(lastClickedPoint_);
    // addItem(item);
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
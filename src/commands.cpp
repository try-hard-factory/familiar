#include "commands.h"
#include "canvasscene.h"
#include "moveitem.h"
#include "selector.h"

#include <QDebug>
#include <QGraphicsScene>

// ============================================================================
// InsertItemsCommand
// ============================================================================
InsertItemsCommand::InsertItemsCommand(CanvasScene* scene,
                                       const QList<IBaseItem*>& items,
                                       const QPointF& position,
                                       bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Insert items"))
    , scene_(scene)
    , position_(position)
    , ignoreFirstRedo_(ignoreFirstRedo)
{
    items_.reserve(items.size());
    for (auto* item : items) {
        items_.append(item);
    }
}

void InsertItemsCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }

    // Calculate offset if position is specified
    if (!position_.isNull()) {
        QList<QGraphicsItem*> graphicsItems;
        for (auto* item : items_) {
            graphicsItems.append(dynamic_cast<QGraphicsItem*>(item));
        }
        QRectF rect = scene_->itemsBoundingRect(false, graphicsItems);

        for (int i = 0; i < items_.size(); ++i) {
            auto* item = dynamic_cast<QGraphicsItem*>(items_[i]);
            oldPositions_.append(item->pos());
            QPointF newPos = item->pos() + position_ - rect.center();
            item->setPos(newPos);
        }
    }

    scene_->clearSelection();
    for (auto* item : items_) {
        auto* graphicsItem = dynamic_cast<QGraphicsItem*>(item);
        scene_->addItem(graphicsItem);
        graphicsItem->setSelected(true);
    }
}

void InsertItemsCommand::undo()
{
    scene_->clearSelection();
    for (auto* item : items_) {
        auto* graphicsItem = dynamic_cast<QGraphicsItem*>(item);
        scene_->removeItem(graphicsItem);
    }

    // Restore old positions if we modified them
    if (!position_.isNull() && !oldPositions_.isEmpty()) {
        for (int i = 0; i < items_.size(); ++i) {
            auto* item = dynamic_cast<QGraphicsItem*>(items_[i]);
            item->setPos(oldPositions_[i]);
        }
        oldPositions_.clear();
    }
}

// ============================================================================
// DeleteItemsCommand
// ============================================================================
DeleteItemsCommand::DeleteItemsCommand(CanvasScene* scene,
                                       const QList<QGraphicsItem*>& items)
    : QUndoCommand(QObject::tr("Delete items"))
    , scene_(scene)
    , items_(items)
{
}

void DeleteItemsCommand::redo()
{
    for (auto* item : items_) {
        scene_->removeItem(item);
    }
}

void DeleteItemsCommand::undo()
{
    scene_->clearSelection();
    for (auto* item : items_) {
        item->setSelected(true);
        scene_->addItem(item);
    }
}

// ============================================================================
// MoveItemsByCommand
// ============================================================================
MoveItemsByCommand::MoveItemsByCommand(const QList<QGraphicsItem*>& items,
                                       const QPointF& delta,
                                       bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Move items"))
    , items_(items)
    , delta_(delta)
    , ignoreFirstRedo_(ignoreFirstRedo)
{
}

void MoveItemsByCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    for (auto* item : items_) {
        item->moveBy(delta_.x(), delta_.y());
    }
}

void MoveItemsByCommand::undo()
{
    for (auto* item : items_) {
        item->moveBy(-delta_.x(), -delta_.y());
    }
}

// ============================================================================
// ScaleItemsByCommand
// ============================================================================
ScaleItemsByCommand::ScaleItemsByCommand(const QList<QGraphicsItem*>& items,
                                         qreal factor,
                                         const QPointF& anchor,
                                         bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Scale items"))
    , items_(items)
    , factor_(factor)
    , anchor_(anchor)
    , ignoreFirstRedo_(ignoreFirstRedo)
{
}

void ScaleItemsByCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setScale(item->scale() * factor_,
                           item->mapFromScene(anchor_));
    }
}

void ScaleItemsByCommand::undo()
{
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setScale(item->scale() / factor_,
                           item->mapFromScene(anchor_));
    }
}

// ============================================================================
// RotateItemsByCommand
// ============================================================================
RotateItemsByCommand::RotateItemsByCommand(const QList<QGraphicsItem*>& items,
                                           qreal delta,
                                           const QPointF& anchor,
                                           bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Rotate items"))
    , items_(items)
    , delta_(delta)
    , anchor_(anchor)
    , ignoreFirstRedo_(ignoreFirstRedo)
{
}

void RotateItemsByCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setRotation(item->rotation() + delta_ * baseItem->flip(),
                              item->mapFromScene(anchor_));
    }
}

void RotateItemsByCommand::undo()
{
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setRotation(item->rotation() - delta_ * baseItem->flip(),
                              item->mapFromScene(anchor_));
    }
}

// ============================================================================
// NormalizeItemsCommand
// ============================================================================
NormalizeItemsCommand::NormalizeItemsCommand(const QList<QGraphicsItem*>& items,
                                             const QList<qreal>& scaleFactors)
    : QUndoCommand(QObject::tr("Normalize items"))
    , items_(items)
    , scaleFactors_(scaleFactors)
{
}

void NormalizeItemsCommand::redo()
{
    oldScaleFactors_.clear();
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        oldScaleFactors_.append(item->scale());
        baseItem->setScale(item->scale() * scaleFactors_[i], baseItem->center());
    }
}

void NormalizeItemsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setScale(oldScaleFactors_[i], baseItem->center());
    }
}

// ============================================================================
// FlipItemsCommand
// ============================================================================
FlipItemsCommand::FlipItemsCommand(const QList<QGraphicsItem*>& items,
                                   const QPointF& anchor,
                                   bool vertical)
    : QUndoCommand(QObject::tr("Flip items"))
    , items_(items)
    , anchor_(anchor)
    , vertical_(vertical)
{
}

void FlipItemsCommand::redo()
{
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->do_flip(vertical_, item->mapFromScene(anchor_));
    }
}

void FlipItemsCommand::undo()
{
    // Flip is its own inverse, so redo() again
    redo();
}

// ============================================================================
// ResetScaleCommand
// ============================================================================
ResetScaleCommand::ResetScaleCommand(const QList<QGraphicsItem*>& items)
    : QUndoCommand(QObject::tr("Reset Scale"))
    , items_(items)
{
}

void ResetScaleCommand::redo()
{
    oldScaleFactors_.clear();
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        oldScaleFactors_.append(item->scale());
        baseItem->setScale(1, baseItem->center());
    }
}

void ResetScaleCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setScale(oldScaleFactors_[i], baseItem->center());
    }
}

// ============================================================================
// ResetRotationCommand
// ============================================================================
ResetRotationCommand::ResetRotationCommand(const QList<QGraphicsItem*>& items)
    : QUndoCommand(QObject::tr("Reset Rotation"))
    , items_(items)
{
}

void ResetRotationCommand::redo()
{
    oldRotations_.clear();
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        oldRotations_.append(item->rotation());
        baseItem->setRotation(0, baseItem->center());
    }
}

void ResetRotationCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->setRotation(oldRotations_[i], baseItem->center());
    }
}

// ============================================================================
// ResetFlipCommand
// ============================================================================
ResetFlipCommand::ResetFlipCommand(const QList<QGraphicsItem*>& items)
    : QUndoCommand(QObject::tr("Reset Flip"))
    , items_(items)
{
}

void ResetFlipCommand::redo()
{
    oldFlips_.clear();
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        oldFlips_.append(baseItem->flip());

        if (baseItem->flip() == -1) {
            baseItem->do_flip(false, baseItem->center());
        }
    }
}

void ResetFlipCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        if (oldFlips_[i] == -1) {
            auto* item = items_[i];
            auto* baseItem = dynamic_cast<IBaseItem*>(item);
            baseItem->do_flip(false, baseItem->center());
        }
    }
}

// ============================================================================
// ResetCropCommand
// ============================================================================
ResetCropCommand::ResetCropCommand(const QList<IBaseItem*>& items)
    : QUndoCommand(QObject::tr("Reset Crop"))
{
    // Filter only croppable items
    for (auto* item : items) {
        if (item->is_image()) {
            items_.append((PixmapItem*)item);
        }
    }
}

void ResetCropCommand::redo()
{
    oldCrops_.clear();
    for (auto* item : items_) {
        oldCrops_.append(item->crop());
        item->reset_crop();
    }
}

void ResetCropCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        items_[i]->set_crop(oldCrops_[i]);
    }
}

// ============================================================================
// ResetTransformsCommand
// ============================================================================
ResetTransformsCommand::ResetTransformsCommand(const QList<IBaseItem*>& items)
    : QUndoCommand(QObject::tr("Reset All Transformations"))
    , items_(items)
{
}

void ResetTransformsCommand::redo()
{
    oldValues_.clear();
    for (auto* baseItem : items_) {
        auto* item = dynamic_cast<QGraphicsItem*>(baseItem);

        TransformValues values;
        values.scale = item->scale();
        values.rotation = item->rotation();
        values.flip = baseItem->flip();
        values.hasCrop = baseItem->is_image();

        if (values.hasCrop) {
            auto* pixmapItem = dynamic_cast<PixmapItem*>(baseItem);
            if (pixmapItem) {
                values.crop = pixmapItem->crop();
                pixmapItem->reset_crop();
            }
        }

        oldValues_.append(values);

        baseItem->setScale(1, baseItem->center());
        baseItem->setRotation(0, baseItem->center());
        if (baseItem->flip() == -1) {
            baseItem->do_flip(false, baseItem->center());
        }
    }
}

void ResetTransformsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* baseItem = items_[i];
        const TransformValues& old = oldValues_[i];

        baseItem->setScale(old.scale, baseItem->center());
        baseItem->setRotation(old.rotation, baseItem->center());
        if (old.flip == -1) {
            baseItem->do_flip(false, baseItem->center());
        }

        if (old.hasCrop) {
            auto* pixmapItem = dynamic_cast<PixmapItem*>(baseItem);
            if (pixmapItem) {
                pixmapItem->set_crop(old.crop);
            }
        }
    }
}

// ============================================================================
// ArrangeItemsCommand
// ============================================================================
ArrangeItemsCommand::ArrangeItemsCommand(CanvasScene* scene,
                                         const QList<QGraphicsItem*>& items,
                                         const QList<QPointF>& positions)
    : QUndoCommand(QObject::tr("Arrange items"))
    , scene_(scene)
    , items_(items)
    , positions_(positions)
{
}

void ArrangeItemsCommand::redo()
{
    oldPositions_.clear();
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        oldPositions_.append(item->pos());

        QPointF origTopLeft = item->mapToScene(QPointF(0, 0));
        QRectF itemRect = scene_->itemsBoundingRect(false, {item});
        QPointF rectTopLeft = itemRect.topLeft();

        item->setPos(positions_[i] + origTopLeft - rectTopLeft);
    }
}

void ArrangeItemsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        items_[i]->setPos(oldPositions_[i]);
    }
}

// ============================================================================
// CropItemCommand
// ============================================================================
CropItemCommand::CropItemCommand(PixmapItem* item, const QRectF& crop)
    : QUndoCommand(QObject::tr("Crop item"))
    , item_(item)
    , crop_(crop)
{
}

void CropItemCommand::redo()
{
    oldCrop_ = item_->crop();
    item_->set_crop(crop_);
}

void CropItemCommand::undo()
{
    item_->set_crop(oldCrop_);
}

// ============================================================================
// ChangeOpacityCommand
// ============================================================================
ChangeOpacityCommand::ChangeOpacityCommand(const QList<QGraphicsItem*>& items,
                                           qreal opacity,
                                           bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Change opacity"))
    , items_(items)
    , opacity_(opacity)
    , ignoreFirstRedo_(ignoreFirstRedo)
{
    for (auto* item : items_) {
        oldOpacities_.append(item->opacity());
    }
}

void ChangeOpacityCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    for (auto* item : items_) {
        item->setOpacity(opacity_);
    }
}

void ChangeOpacityCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        items_[i]->setOpacity(oldOpacities_[i]);
    }
}

// ============================================================================
// ChangeTextCommand
// ============================================================================
ChangeTextCommand::ChangeTextCommand(TextItem* item,
                                     const QString& newText,
                                     const QString& oldText)
    : QUndoCommand(QObject::tr("Change text"))
    , item_(item)
    , newText_(newText)
    , oldText_(oldText)
{
}

void ChangeTextCommand::redo()
{
    item_->setPlainText(newText_);
}

void ChangeTextCommand::undo()
{
    item_->setPlainText(oldText_);
}

// ============================================================================
// ToggleGrayscaleCommand
// ============================================================================
ToggleGrayscaleCommand::ToggleGrayscaleCommand(const QList<PixmapItem*>& items, bool grayscale)
    : QUndoCommand(QObject::tr("Toggle Grayscale"))
    , items_(items)
    , grayscale_(grayscale)
{
    for (auto* item : items_) {
        oldGrayscales_.append(item->grayscale());
    }
}

void ToggleGrayscaleCommand::redo()
{
    for (auto* item : items_) {
        item->setGrayscale(grayscale_);
    }
}

void ToggleGrayscaleCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        items_[i]->setGrayscale(oldGrayscales_[i]);
    }
}

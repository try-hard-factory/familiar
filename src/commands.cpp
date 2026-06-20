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
        // Get the item's mixin to access setScale with anchor
        // For now, use the item's transform
        QPointF sceneAnchor = item->mapToScene(anchor_);
        qreal newScale = item->scale() * factor_;

        // Apply scale around anchor point
        QPointF prev = item->mapToScene(anchor_);
        item->setScale(newScale);
        QPointF diff = item->mapToScene(anchor_) - prev;
        item->setPos(item->pos() - diff);
    }
}

void ScaleItemsByCommand::undo()
{
    for (auto* item : items_) {
        QPointF prev = item->mapToScene(anchor_);
        qreal newScale = item->scale() / factor_;
        item->setScale(newScale);
        QPointF diff = item->mapToScene(anchor_) - prev;
        item->setPos(item->pos() - diff);
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
        // Get flip factor from transform matrix m11
        qreal flip = item->transform().m11() < 0 ? -1.0 : 1.0;
        qreal newRotation = item->rotation() + delta_ * flip;

        QPointF prev = item->mapToScene(anchor_);
        item->setRotation(std::fmod(newRotation, 360.0));
        QPointF diff = item->mapToScene(anchor_) - prev;
        item->setPos(item->pos() - diff);
    }
}

void RotateItemsByCommand::undo()
{
    for (auto* item : items_) {
        qreal flip = item->transform().m11() < 0 ? -1.0 : 1.0;
        qreal newRotation = item->rotation() - delta_ * flip;

        QPointF prev = item->mapToScene(anchor_);
        item->setRotation(std::fmod(newRotation, 360.0));
        QPointF diff = item->mapToScene(anchor_) - prev;
        item->setPos(item->pos() - diff);
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
        oldScaleFactors_.append(item->scale());

        // Apply scale around center
        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);
        item->setScale(item->scale() * scaleFactors_[i]);
        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
    }
}

void NormalizeItemsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);
        item->setScale(oldScaleFactors_[i]);
        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
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
        // Get current flip state from transform
        qreal currentFlip = item->transform().m11();
        QPointF localAnchor = item->mapFromScene(anchor_);

        // Apply flip by scaling X by -1
        QPointF prev = item->mapToScene(localAnchor);
        QTransform transform = item->transform();
        transform.scale(-1, 1);
        item->setTransform(transform);

        // If vertical flip, also rotate 180 degrees
        if (vertical_) {
            item->setRotation(item->rotation() + 180);
        }

        QPointF diff = item->mapToScene(localAnchor) - prev;
        item->setPos(item->pos() - diff);
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
        oldScaleFactors_.append(item->scale());

        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);
        item->setScale(1.0);
        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
    }
}

void ResetScaleCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);
        item->setScale(oldScaleFactors_[i]);
        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
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
        oldRotations_.append(item->rotation());

        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);
        item->setRotation(0);
        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
    }
}

void ResetRotationCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);
        item->setRotation(oldRotations_[i]);
        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
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
        qreal flip = item->transform().m11() < 0 ? -1.0 : 1.0;
        oldFlips_.append(flip);

        if (flip == -1) {
            QPointF center = item->boundingRect().center();
            QPointF prev = item->mapToScene(center);
            QTransform transform = item->transform();
            transform.scale(-1, 1);
            item->setTransform(transform);
            QPointF diff = item->mapToScene(center) - prev;
            item->setPos(item->pos() - diff);
        }
    }
}

void ResetFlipCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        if (oldFlips_[i] == -1) {
            auto* item = items_[i];
            QPointF center = item->boundingRect().center();
            QPointF prev = item->mapToScene(center);
            QTransform transform = item->transform();
            transform.scale(-1, 1);
            item->setTransform(transform);
            QPointF diff = item->mapToScene(center) - prev;
            item->setPos(item->pos() - diff);
        }
    }
}

// ============================================================================
// ResetCropCommand
// ============================================================================
ResetCropCommand::ResetCropCommand(const QList<PixmapItem*>& items)
    : QUndoCommand(QObject::tr("Reset Crop"))
{
    // Filter only croppable items
    for (auto* item : items) {
        if (item->is_croppable()) {
            items_.append(item);
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
        if (!item)
            continue;

        TransformValues values;
        values.scale = item->scale();
        values.rotation = item->rotation();
        values.flip = item->transform().m11() < 0 ? -1.0 : 1.0;
        values.hasCrop = baseItem->is_croppable();

        // Get crop if applicable
        if (values.hasCrop) {
            auto* pixmapItem = dynamic_cast<PixmapItem*>(baseItem);
            if (pixmapItem) {
                values.crop = pixmapItem->crop();
                pixmapItem->reset_crop();
            }
        }

        oldValues_.append(values);

        // Apply reset transforms
        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);

        item->setScale(1.0);
        item->setRotation(0);

        // Reset flip if needed
        if (values.flip == -1) {
            QTransform transform = item->transform();
            transform.scale(-1, 1);
            item->setTransform(transform);
        }

        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
    }
}

void ResetTransformsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* baseItem = items_[i];
        auto* item = dynamic_cast<QGraphicsItem*>(baseItem);
        if (!item)
            continue;

        const TransformValues& old = oldValues_[i];

        QPointF center = item->boundingRect().center();
        QPointF prev = item->mapToScene(center);

        item->setScale(old.scale);
        item->setRotation(old.rotation);

        // Restore flip if needed
        if (old.flip == -1) {
            qreal currentFlip = item->transform().m11() < 0 ? -1.0 : 1.0;
            if (currentFlip != -1) {
                QTransform transform = item->transform();
                transform.scale(-1, 1);
                item->setTransform(transform);
            }
        }

        // Restore crop if applicable
        if (old.hasCrop) {
            auto* pixmapItem = dynamic_cast<PixmapItem*>(baseItem);
            if (pixmapItem) {
                pixmapItem->set_crop(old.crop);
            }
        }

        QPointF diff = item->mapToScene(center) - prev;
        item->setPos(item->pos() - diff);
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

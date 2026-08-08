#include "commands.h"
#include "canvasscene.h"
#include "moveitem.h"
#include "selector.h"

#include <QGraphicsScene>

// ============================================================================
// InsertItemsCommand
// ============================================================================
InsertItemsCommand::InsertItemsCommand(CanvasScene* scene,
                                       const QList<IBaseItem*>& items,
                                       std::optional<QPointF> position,
                                       bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Insert items"))
    , scene_(scene)
    , position_(position)
    , ignoreFirstRedo_(ignoreFirstRedo)
{
    items_.reserve(items.size());
    ownedRefs_.reserve(items.size());
    for (auto* item : items) {
        items_.append(item);
        ownedRefs_.append(item->acquireShared());
    }
}

void InsertItemsCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }

    scene_->deselect_all_items();
    if (position_) {
        oldPositions_.clear();
        QList<QGraphicsItem*> graphicsItems;
        for (auto* item : items_) {
            graphicsItems.append(dynamic_cast<QGraphicsItem*>(item));
        }
        QRectF rect = scene_->itemsBoundingRect(false, graphicsItems);

        for (int i = 0; i < items_.size(); ++i) {
            auto* item = dynamic_cast<QGraphicsItem*>(items_[i]);
            oldPositions_.append(item->pos());
            QPointF newPos = item->pos() + *position_ - rect.center();
            item->setPos(newPos);
        }
    }

    for (auto* item : items_) {
        auto* graphicsItem = dynamic_cast<QGraphicsItem*>(item);
        scene_->addItem(graphicsItem);
        graphicsItem->setSelected(true);
        item->bring_to_front();
    }
}

void InsertItemsCommand::undo()
{
    scene_->deselect_all_items();
    for (auto* item : items_) {
        auto* graphicsItem = dynamic_cast<QGraphicsItem*>(item);
        scene_->removeItem(graphicsItem);
    }

    if (position_) {
        for (int i = 0; i < items_.size(); ++i) {
            auto* item = dynamic_cast<QGraphicsItem*>(items_[i]);
            item->setPos(oldPositions_[i]);
        }
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
    ownedRefs_.reserve(items.size());
    for (auto* item : items) {
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
            ownedRefs_.append(baseItem->acquireShared());
        }
    }
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
{}

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
{}

void ScaleItemsByCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(item->scale() * factor_,
                            item->mapFromScene(anchor_));
    }
}

void ScaleItemsByCommand::undo()
{
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(item->scale() / factor_,
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
{}

void RotateItemsByCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(item->rotation() + delta_ * baseItem->flip(),
                               item->mapFromScene(anchor_));
    }
}

void RotateItemsByCommand::undo()
{
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(item->rotation() - delta_ * baseItem->flip(),
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
{}

void NormalizeItemsCommand::redo()
{
    oldScaleFactors_.clear();
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        oldScaleFactors_.append(item->scale());
        baseItem->set_scale(item->scale() * scaleFactors_[i],
                            baseItem->center());
    }
}

void NormalizeItemsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(oldScaleFactors_[i], baseItem->center());
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
{}

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
ResetScaleCommand::ResetScaleCommand(const QList<QGraphicsItem*>& items,
                                     const QPointF& anchor)
    : QUndoCommand(QObject::tr("Reset Scale"))
    , items_(items)
    , anchor_(anchor)
{}

void ResetScaleCommand::redo()
{
    oldScaleFactors_.clear();
    for (auto* item : items_) {
        oldScaleFactors_.append(item->scale());
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(1, item->mapFromScene(anchor_));
    }
}

void ResetScaleCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(oldScaleFactors_[i], item->mapFromScene(anchor_));
    }
}

// ============================================================================
// ResetRotationCommand
// ============================================================================
ResetRotationCommand::ResetRotationCommand(const QList<QGraphicsItem*>& items,
                                           const QPointF& anchor)
    : QUndoCommand(QObject::tr("Reset Rotation"))
    , items_(items)
    , anchor_(anchor)
{}

void ResetRotationCommand::redo()
{
    oldRotations_.clear();
    for (auto* item : items_) {
        oldRotations_.append(item->rotation());
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(0, item->mapFromScene(anchor_));
    }
}

void ResetRotationCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* item = items_[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(oldRotations_[i], item->mapFromScene(anchor_));
    }
}

// ============================================================================
// ResetFlipCommand
// ============================================================================
ResetFlipCommand::ResetFlipCommand(const QList<QGraphicsItem*>& items,
                                   const QPointF& anchor)
    : QUndoCommand(QObject::tr("Reset Flip"))
    , items_(items)
    , anchor_(anchor)
{}

void ResetFlipCommand::redo()
{
    oldFlips_.clear();
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        oldFlips_.append(baseItem->flip());

        if (baseItem->flip() == -1) {
            baseItem->do_flip(false, item->mapFromScene(anchor_));
        }
    }
}

void ResetFlipCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        if (oldFlips_[i] == -1) {
            auto* item = items_[i];
            auto* baseItem = dynamic_cast<IBaseItem*>(item);
            baseItem->do_flip(false, item->mapFromScene(anchor_));
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
            items_.append((PixmapItem*) item);
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
ResetTransformsCommand::ResetTransformsCommand(const QList<IBaseItem*>& items,
                                               const QPointF& anchor)
    : QUndoCommand(QObject::tr("Reset All Transformations"))
    , items_(items)
    , anchor_(anchor)
{}

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

        const QPointF localAnchor = item->mapFromScene(anchor_);
        baseItem->set_scale(1, localAnchor);
        baseItem->set_rotation(0, localAnchor);
        if (baseItem->flip() == -1) {
            baseItem->do_flip(false, localAnchor);
        }
    }
}

void ResetTransformsCommand::undo()
{
    for (int i = 0; i < items_.size(); ++i) {
        auto* baseItem = items_[i];
        auto* item = dynamic_cast<QGraphicsItem*>(baseItem);
        const TransformValues& old = oldValues_[i];
        const QPointF localAnchor = item->mapFromScene(anchor_);

        baseItem->set_scale(old.scale, localAnchor);
        baseItem->set_rotation(old.rotation, localAnchor);
        if (old.flip == -1) {
            baseItem->do_flip(false, localAnchor);
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
{}

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
{}

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
// ResizeTextFieldCommand
// ============================================================================
ResizeTextFieldCommand::ResizeTextFieldCommand(TextItem* item,
                                               qreal newWidth,
                                               qreal newHeight,
                                               qreal oldWidth,
                                               qreal oldHeight,
                                               bool anchorRight,
                                               bool anchorBottom,
                                               bool ignoreFirstRedo)
    : QUndoCommand(QObject::tr("Resize text field"))
    , item_(item)
    , newWidth_(newWidth)
    , newHeight_(newHeight)
    , oldWidth_(oldWidth)
    , oldHeight_(oldHeight)
    , anchorRight_(anchorRight)
    , anchorBottom_(anchorBottom)
    , ignoreFirstRedo_(ignoreFirstRedo)
{}

void ResizeTextFieldCommand::redo()
{
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    item_->resize_field(newWidth_, newHeight_, anchorRight_, anchorBottom_);
}

void ResizeTextFieldCommand::undo()
{
    item_->resize_field(oldWidth_, oldHeight_, anchorRight_, anchorBottom_);
}

// ============================================================================
// ChangeTextCommand
// ============================================================================
ChangeTextCommand::ChangeTextCommand(TextItem* item,
                                     const QString& newHtml,
                                     const QString& oldHtml,
                                     const QColor& newFillColor,
                                     const QColor& oldFillColor)
    : QUndoCommand(QObject::tr("Change text"))
    , item_(item)
    , newHtml_(newHtml)
    , oldHtml_(oldHtml)
    , newFillColor_(newFillColor)
    , oldFillColor_(oldFillColor)
{}

void ChangeTextCommand::redo()
{
    item_->setHtml(newHtml_);
    item_->set_fill_color(newFillColor_);
}

void ChangeTextCommand::undo()
{
    item_->setHtml(oldHtml_);
    item_->set_fill_color(oldFillColor_);
}

// ============================================================================
// ChangeGroupFillColorCommand
// ============================================================================
ChangeGroupFillColorCommand::ChangeGroupFillColorCommand(GroupItem* item,
                                                         const QColor& newColor,
                                                         const QColor& oldColor)
    : QUndoCommand(QObject::tr("Change group fill color"))
    , item_(item)
    , newColor_(newColor)
    , oldColor_(oldColor)
{}

void ChangeGroupFillColorCommand::redo()
{
    item_->set_fill_color(newColor_);
}

void ChangeGroupFillColorCommand::undo()
{
    item_->set_fill_color(oldColor_);
}

// ============================================================================
// ToggleGrayscaleCommand
// ============================================================================
ToggleGrayscaleCommand::ToggleGrayscaleCommand(const QList<PixmapItem*>& items)
    : QUndoCommand(QObject::tr("Toggle Grayscale"))
    , items_(items)
{}

void ToggleGrayscaleCommand::redo()
{
    for (auto* item : items_) {
        item->setGrayscale(!item->grayscale());
    }
}

void ToggleGrayscaleCommand::undo()
{
    // Self-inverse: applying the same per-item inversion again exactly
    // undoes it, like FlipItemsCommand.
    redo();
}

// ============================================================================
// GroupCommand
// ============================================================================
GroupCommand::GroupCommand(CanvasScene* scene,
                           GroupItem* group,
                           const QList<QGraphicsItem*>& members)
    : QUndoCommand(QObject::tr("Group items"))
    , scene_(scene)
    , group_(group)
    , members_(members)
    , groupRef_(group->acquireShared())
{}

void GroupCommand::redo()
{
    scene_->deselect_all_items();
    // Sits BEHIND every member it contains - set below their current z,
    // not bring_to_front() (InsertItemsCommand's usual move for newly
    // inserted content): a group is a background, not new content on top.
    qreal minZ = members_.isEmpty() ? 0 : members_.first()->zValue();
    for (auto* item : members_)
        minZ = qMin(minZ, item->zValue());
    scene_->addItem(group_);
    group_->set_z_value(minZ - scene_->Z_STEP);
    group_->setSelected(true);
}

void GroupCommand::undo()
{
    scene_->deselect_all_items();
    scene_->removeItem(group_);
    for (auto* item : members_)
        item->setSelected(true);
}

// ============================================================================
// UngroupCommand
// ============================================================================
UngroupCommand::UngroupCommand(CanvasScene* scene, GroupItem* group)
    : QUndoCommand(QObject::tr("Ungroup"))
    , scene_(scene)
    , group_(group)
    , members_(group->resolve_children())
    , groupRef_(group->acquireShared())
{}

void UngroupCommand::redo()
{
    scene_->deselect_all_items();
    scene_->removeItem(group_);
    for (auto* item : members_)
        item->setSelected(true);
}

void UngroupCommand::undo()
{
    scene_->deselect_all_items();
    scene_->addItem(group_);
    group_->setSelected(true);
}

// ============================================================================
// RemoveFromGroupCommand
// ============================================================================
RemoveFromGroupCommand::RemoveFromGroupCommand(GroupItem* group,
                                               const QUuid& memberUid)
    : QUndoCommand(QObject::tr("Remove from group"))
    , group_(group)
    , memberUid_(memberUid)
{}

void RemoveFromGroupCommand::redo()
{
    group_->remove_child_id(memberUid_);
}

void RemoveFromGroupCommand::undo()
{
    group_->add_child_id(memberUid_);
}

// ============================================================================
// AddToGroupCommand
// ============================================================================
AddToGroupCommand::AddToGroupCommand(CanvasScene* scene,
                                     GroupItem* group,
                                     const QList<QGraphicsItem*>& members)
    : QUndoCommand(QObject::tr("Add to group"))
    , scene_(scene)
    , group_(group)
    , members_(members)
{
    for (auto* item : members_) {
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item))
            memberUids_.append(baseItem->uid());
    }
}

void AddToGroupCommand::redo()
{
    scene_->deselect_all_items();
    for (int i = 0; i < memberUids_.size(); ++i) {
        group_->add_child_id(memberUids_[i]);
        // A group's fill always sits BEHIND its members (see
        // GroupCommand::redo()) - a loose item folded into an ALREADY
        // EXISTING group keeps whatever z it already had, which can
        // easily be below the group's own z (e.g. it's an older item
        // than the group, or the group got raised since), making it
        // render under the fill and effectively vanish the instant it
        // joins. Bump it just above the group if it isn't already -
        // same +Z_STEP convention GroupCommand::redo() uses in reverse.
        QGraphicsItem* item = members_[i];
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
            if (item->zValue() <= group_->zValue())
                baseItem->set_z_value(group_->zValue() + scene_->Z_STEP);
        }
    }
    // fit_to_contain_children() (moveitem.h) otherwise only runs
    // reactively from CanvasScene::on_change() (the scene's changed()
    // signal) - without an explicit call here, the group's rect stays
    // stale (not yet grown around the newly joined member) until some
    // LATER, unrelated scene change happens to trigger a refit. If the
    // joined item is far from the group's existing members, that refit
    // can then look like the group randomly exploding in size on some
    // later click that has nothing to do with grouping. Fit immediately
    // so the new, real footprint is visible right away instead.
    group_->fit_to_contain_children();
    group_->setSelected(true);
}

void AddToGroupCommand::undo()
{
    scene_->deselect_all_items();
    for (const QUuid& uid : memberUids_)
        group_->remove_child_id(uid);
    // Same reasoning as redo() above - shrink back down immediately
    // instead of leaving a stale, too-large rect until something else
    // happens to trigger the next refit.
    group_->fit_to_contain_children();
    // Restore the exact pre-Group() selection (group + the loose items),
    // not just the group alone - matches GroupCommand::undo()/
    // UngroupCommand::redo() both reselecting the members they touched.
    group_->setSelected(true);
    for (auto* item : members_)
        item->setSelected(true);
}

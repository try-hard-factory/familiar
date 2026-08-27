#include "commands.h"
#include "canvasscene.h"
#include "moveitem.h"
#include "selector.h"

#include <QGraphicsScene>

#include "log/log.h"
using namespace familiar::log;

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
    FLOG_DEBUG(Ch::Undo, "InsertItemsCommand::redo() ({} item(s))", items_.size());
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
        // An attached item riding along in this same batch (e.g. a
        // paste that copied a picture together with its attached
        // notes/pictures) doesn't join the Qt multi-selection - it
        // already gets its own 1px selection-color outline whenever its
        // anchor is selected (TextItem::paint()/PixmapItem::paint()'s
        // own attached-item indicator), which the anchor's own
        // setSelected(true) below triggers automatically. A plain item
        // (attachedToUid().isNull()) - including a copied GROUP and its
        // non-attached members - keeps the previous behavior unchanged.
        if (item->attachedToUid().isNull()) {
            graphicsItem->setSelected(true);
        }
        item->bring_to_front();
    }
}

void InsertItemsCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "InsertItemsCommand::undo() ({} item(s))", items_.size());
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
    owningGroups_.reserve(items.size());
    for (auto* item : items) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        if (baseItem) {
            ownedRefs_.append(baseItem->acquireShared());
            owningGroups_.append(scene_->find_owning_group(baseItem->uid()));
        } else {
            owningGroups_.append(nullptr);
        }
    }
}

void DeleteItemsCommand::redo()
{
    FLOG_DEBUG(Ch::Undo, "DeleteItemsCommand::redo() ({} item(s))", items_.size());
    for (int i = 0; i < items_.size(); ++i) {
        QGraphicsItem* item = items_[i];
        // Prune the group membership BEFORE removeItem() below, not
        // after - a grouped member's uid otherwise lingers in GroupItem::
        // childIds_ (harmless on its own, find_by_uid()-based resolution
        // already skips a dangling uid - see resolve_children()'s own
        // comment) but resolve_children()'s CACHE (resolvedChildren_)
        // only gets invalidated by an explicit membership change, not by
        // a plain scene removal - so without this, HierarchyPanel::
        // rebuild_() kept re-adding the deleted item's C++ object (still
        // alive - kept around for undo, see ownedRefs_) straight back
        // into the tree via the group's stale cache. Only reproduced for
        // a GROUPED item, never a top-level one (which rebuild_() reads
        // straight off scene_->items(), no cache involved).
        if (GroupItem* group = owningGroups_[i]) {
            if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
                group->remove_child_id(baseItem->uid());
            }
        }
        scene_->removeItem(item);
    }
}

void DeleteItemsCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "DeleteItemsCommand::undo() ({} item(s))", items_.size());
    // addItem() BEFORE setSelected(), not after - setSelected(true)
    // fires itemChange(ItemSelectedChange) synchronously, which reaches
    // ItemMixin::on_selected_change() (moveitem.h) and asserts this->
    // scene() is non-null. Selecting an item that isn't attached to any
    // scene yet crashed (Q_ASSERT abort in a debug build) - confirmed
    // via a real stack trace, undoing a delete.
    scene_->clearSelection();
    for (int i = 0; i < items_.size(); ++i) {
        QGraphicsItem* item = items_[i];
        scene_->addItem(item);
        item->setSelected(true);
        // Mirrors redo()'s own pruning - restores membership in the
        // SAME group (works even if that group was ALSO deleted as part
        // of this same command and is being restored earlier in this
        // same loop, since with_related_items() always orders a group
        // before its own members).
        if (GroupItem* group = owningGroups_[i]) {
            if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
                group->add_child_id(baseItem->uid());
            }
        }
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
    FLOG_DEBUG(Ch::Undo, "MoveItemsByCommand::redo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "MoveItemsByCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ScaleItemsByCommand::redo() ({} item(s))", items_.size());
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    // begin/end_group_batch(): undo/redo replays the exact same flat
    // selection_action_items() list the live drag used - an attached
    // note sharing a group with its picture in this
    // list needs the same double-apply guard here as the live drag gets
    // in selector.h (see PixmapItem::itemChange()'s comment).
    auto* scene = items_.isEmpty()
                      ? nullptr
                      : dynamic_cast<CanvasScene*>(items_.first()->scene());
    if (scene) {
        scene->begin_group_batch();
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(item->scale() * factor_,
                            item->mapFromScene(anchor_));
    }
    if (scene) {
        scene->end_group_batch();
    }
}

void ScaleItemsByCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ScaleItemsByCommand::undo() ({} item(s))", items_.size());
    auto* scene = items_.isEmpty()
                      ? nullptr
                      : dynamic_cast<CanvasScene*>(items_.first()->scene());
    if (scene) {
        scene->begin_group_batch();
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(item->scale() / factor_,
                            item->mapFromScene(anchor_));
    }
    if (scene) {
        scene->end_group_batch();
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
    FLOG_DEBUG(Ch::Undo, "RotateItemsByCommand::redo() ({} item(s))", items_.size());
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    // Same reasoning as ScaleItemsByCommand::redo() above.
    auto* scene = items_.isEmpty()
                      ? nullptr
                      : dynamic_cast<CanvasScene*>(items_.first()->scene());
    if (scene) {
        scene->begin_group_batch();
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(item->rotation() + delta_ * baseItem->flip(),
                               item->mapFromScene(anchor_));
    }
    if (scene) {
        scene->end_group_batch();
    }
}

void RotateItemsByCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "RotateItemsByCommand::undo() ({} item(s))", items_.size());
    auto* scene = items_.isEmpty()
                      ? nullptr
                      : dynamic_cast<CanvasScene*>(items_.first()->scene());
    if (scene) {
        scene->begin_group_batch();
    }
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(item->rotation() - delta_ * baseItem->flip(),
                               item->mapFromScene(anchor_));
    }
    if (scene) {
        scene->end_group_batch();
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
    FLOG_DEBUG(Ch::Undo, "NormalizeItemsCommand::redo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "NormalizeItemsCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "FlipItemsCommand::redo() ({} item(s))", items_.size());
    for (auto* item : items_) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->do_flip(vertical_, item->mapFromScene(anchor_));
    }
}

void FlipItemsCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "FlipItemsCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetScaleCommand::redo() ({} item(s))", items_.size());
    oldScaleFactors_.clear();
    for (auto* item : items_) {
        oldScaleFactors_.append(item->scale());
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_scale(1, item->mapFromScene(anchor_));
    }
}

void ResetScaleCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ResetScaleCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetRotationCommand::redo() ({} item(s))", items_.size());
    oldRotations_.clear();
    for (auto* item : items_) {
        oldRotations_.append(item->rotation());
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        baseItem->set_rotation(0, item->mapFromScene(anchor_));
    }
}

void ResetRotationCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ResetRotationCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetFlipCommand::redo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetFlipCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetCropCommand::redo() ({} item(s))", items_.size());
    oldCrops_.clear();
    for (auto* item : items_) {
        oldCrops_.append(item->crop());
        item->reset_crop();
    }
}

void ResetCropCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ResetCropCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetTransformsCommand::redo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResetTransformsCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ArrangeItemsCommand::redo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ArrangeItemsCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "CropItemCommand::redo()");
    oldCrop_ = item_->crop();
    item_->set_crop(crop_);
}

void CropItemCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "CropItemCommand::undo()");
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
    FLOG_DEBUG(Ch::Undo, "ChangeOpacityCommand::redo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ChangeOpacityCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "ResizeTextFieldCommand::redo()");
    if (ignoreFirstRedo_) {
        ignoreFirstRedo_ = false;
        return;
    }
    item_->resize_field(newWidth_, newHeight_, anchorRight_, anchorBottom_);
}

void ResizeTextFieldCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ResizeTextFieldCommand::undo()");
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
    FLOG_DEBUG(Ch::Undo, "ChangeTextCommand::redo()");
    item_->setHtml(newHtml_);
    item_->set_fill_color(newFillColor_);
}

void ChangeTextCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ChangeTextCommand::undo()");
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
    FLOG_DEBUG(Ch::Undo, "ChangeGroupFillColorCommand::redo()");
    item_->set_fill_color(newColor_);
}

void ChangeGroupFillColorCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ChangeGroupFillColorCommand::undo()");
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
    FLOG_DEBUG(Ch::Undo, "ToggleGrayscaleCommand::redo() ({} item(s))", items_.size());
    for (auto* item : items_) {
        item->setGrayscale(!item->grayscale());
    }
}

void ToggleGrayscaleCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "ToggleGrayscaleCommand::undo() ({} item(s))", items_.size());
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
    FLOG_DEBUG(Ch::Undo, "GroupCommand::redo() ({} member(s))", members_.size());
    scene_->deselect_all_items();
    // Sits BEHIND every member it contains - set below their current z,
    // not bring_to_front() (InsertItemsCommand's usual move for newly
    // inserted content): a group is a background, not new content on top.
    qreal minZ = members_.isEmpty() ? 0 : members_.first()->zValue();
    for (auto* item : members_) {
        minZ = qMin(minZ, item->zValue());
    }
    scene_->addItem(group_);
    group_->set_z_value(minZ - scene_->Z_STEP);
    group_->setSelected(true);
}

void GroupCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "GroupCommand::undo() ({} member(s))", members_.size());
    scene_->deselect_all_items();
    scene_->removeItem(group_);
    for (auto* item : members_) {
        item->setSelected(true);
    }
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
    FLOG_DEBUG(Ch::Undo, "UngroupCommand::redo() ({} member(s))", members_.size());
    scene_->deselect_all_items();
    scene_->removeItem(group_);
    for (auto* item : members_) {
        item->setSelected(true);
    }
}

void UngroupCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "UngroupCommand::undo() ({} member(s))", members_.size());
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
{
    memberUids_.append(memberUid);
    // A picture's attached items ride along as actual
    // group members now (AddToGroupCommand/group_selection() both fold
    // them in whenever the picture joins) - taking the picture back out
    // has to take them too, or they'd linger as orphaned members of a
    // group their picture no longer belongs to. Index-based, over
    // memberUids_ itself as it grows - a picture can now be attached
    // to another picture (current), so a whole chain has to come
    // along, not just whatever is directly attached to the picture
    // being removed. Same pattern as CanvasScene::with_attached_items().
    if (auto* scene = dynamic_cast<CanvasScene*>(group_->scene())) {
        for (int i = 0; i < memberUids_.size(); ++i) {
            auto* picture = dynamic_cast<PixmapItem*>(
                scene->find_by_uid(memberUids_[i]));
            if (!picture) {
                continue;
            }
            for (QGraphicsItem* attachedItem :
                 scene->find_attached_items(picture->uid())) {
                if (auto* attachedBase = dynamic_cast<IBaseItem*>(
                        attachedItem)) {
                    if (!memberUids_.contains(attachedBase->uid())) {
                        memberUids_.append(attachedBase->uid());
                    }
                }
            }
        }
    }
}

void RemoveFromGroupCommand::redo()
{
    FLOG_DEBUG(Ch::Undo,
              "RemoveFromGroupCommand::redo() ({} member uid(s))",
              memberUids_.size());
    for (const QUuid& uid : memberUids_) {
        group_->remove_child_id(uid);
    }
    // Immediate refit, same reasoning as AddToGroupCommand::redo() (in
    // reverse) - otherwise the group's rect stays stale (too large,
    // still including the just-departed member's old footprint) until
    // some later, unrelated scene change happens to trigger one. Matters
    // most for a nested subgroup leaving its outer group - the outer's
    // boundary needs to shrink right away, not lag behind.
    group_->fit_to_contain_children();
}

void RemoveFromGroupCommand::undo()
{
    FLOG_DEBUG(Ch::Undo,
              "RemoveFromGroupCommand::undo() ({} member uid(s))",
              memberUids_.size());
    for (const QUuid& uid : memberUids_) {
        group_->add_child_id(uid);
    }
    group_->fit_to_contain_children();
}

// ============================================================================
// AddToGroupCommand
// ============================================================================
AddToGroupCommand::AddToGroupCommand(CanvasScene* scene,
                                     GroupItem* group,
                                     const QList<QGraphicsItem*>& members,
                                     bool reselectOnUndo)
    : QUndoCommand(QObject::tr("Add to group"))
    , scene_(scene)
    , group_(group)
    // with_attached_items(): any picture among `members` brings its
    // attached notes in as actual group members too,
    // not just a position/z cascade - covers the "Group" button's
    // fold-into-existing-group path AND drag-drop add/transfer, both of
    // which construct this command.
    , members_(scene->with_attached_items(members))
    , primaryMembers_(members)
    , reselectOnUndo_(reselectOnUndo)
{
    for (auto* item : members_) {
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
            memberUids_.append(baseItem->uid());
        }
    }
}

void AddToGroupCommand::redo()
{
    FLOG_DEBUG(Ch::Undo,
              "AddToGroupCommand::redo() ({} member(s))",
              memberUids_.size());
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
        // An attached note is now folded into
        // members_ too (see constructor's with_attached_items()), so it
        // gets the exact same bump as any other member here - no
        // separate case needed.
        QGraphicsItem* item = members_[i];
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
            if (item->zValue() <= group_->zValue()) {
                baseItem->set_z_value(group_->zValue() + scene_->Z_STEP);
            }
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
    FLOG_DEBUG(Ch::Undo,
              "AddToGroupCommand::undo() ({} member(s))",
              memberUids_.size());
    scene_->deselect_all_items();
    for (const QUuid& uid : memberUids_) {
        group_->remove_child_id(uid);
    }
    // Same reasoning as redo() above - shrink back down immediately
    // instead of leaving a stale, too-large rect until something else
    // happens to trigger the next refit.
    group_->fit_to_contain_children();
    if (reselectOnUndo_) {
        // Restore the exact pre-Group() selection (group + the loose
        // items), not just the group alone - matches GroupCommand::undo()/
        // UngroupCommand::redo() both reselecting the members they
        // touched.
        group_->setSelected(true);
        for (auto* item : members_) {
            item->setSelected(true);
        }
    } else {
        // Drag-drop path: just the dragged item(s), not the group and
        // not any attached notes with_attached_items() folded into
        // members_ - see constructor comment.
        for (auto* item : primaryMembers_) {
            item->setSelected(true);
        }
    }
}

// ============================================================================
// SetAttachedToCommand
// ============================================================================
SetAttachedToCommand::SetAttachedToCommand(IBaseItem* item,
                                           const QUuid& oldUid,
                                           const QUuid& newUid)
    : QUndoCommand(newUid.isNull() ? QObject::tr("Detach")
                                   : QObject::tr("Attach"))
    , item_(item)
    , oldUid_(oldUid)
    , newUid_(newUid)
{}

void SetAttachedToCommand::redo()
{
    FLOG_DEBUG(Ch::Undo, "SetAttachedToCommand::redo()");
    item_->set_attached_to(newUid_);
}

void SetAttachedToCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "SetAttachedToCommand::undo()");
    item_->set_attached_to(oldUid_);
}

// ============================================================================
// RenamePictureCommand
// ============================================================================
RenamePictureCommand::RenamePictureCommand(PixmapItem* item,
                                           const QString& oldName,
                                           const QString& newName)
    : QUndoCommand(QObject::tr("Rename"))
    , item_(item)
    , oldName_(oldName)
    , newName_(newName)
{}

void RenamePictureCommand::redo()
{
    FLOG_DEBUG(Ch::Undo, "RenamePictureCommand::redo()");
    item_->filename_ = newName_;
}

void RenamePictureCommand::undo()
{
    FLOG_DEBUG(Ch::Undo, "RenamePictureCommand::undo()");
    item_->filename_ = oldName_;
}

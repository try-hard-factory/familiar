#pragma once

#include <memory>
#include <optional>
#include <QColor>
#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QUndoCommand>

class CanvasScene;
class IBaseItem;
class PixmapItem;
class TextItem;

// ============================================================================
// InsertItemsCommand - Вставка элементов на сцену
// ============================================================================
class InsertItemsCommand : public QUndoCommand
{
public:
    InsertItemsCommand(CanvasScene* scene,
                       const QList<IBaseItem*>& items,
                       std::optional<QPointF> position = std::nullopt,
                       bool ignoreFirstRedo = false);

    void redo() override;
    void undo() override;

private:
    CanvasScene* scene_;
    QList<IBaseItem*> items_;
    // Shares ownership with CanvasScene::attachedItems_ (while attached)
    // and any other command referencing the same items (e.g. a later
    // DeleteItemsCommand for one of them) - see IBaseItem::acquireShared().
    // Plain RAII: whichever owner is destroyed last frees the item.
    QList<std::shared_ptr<IBaseItem>> ownedRefs_;
    std::optional<QPointF> position_;
    QList<QPointF> oldPositions_;
    bool ignoreFirstRedo_;
};

// ============================================================================
// DeleteItemsCommand - Удаление элементов со сцены
// ============================================================================
class DeleteItemsCommand : public QUndoCommand
{
public:
    DeleteItemsCommand(CanvasScene* scene, const QList<QGraphicsItem*>& items);

    void redo() override;
    void undo() override;

private:
    CanvasScene* scene_;
    QList<QGraphicsItem*> items_;
    // See InsertItemsCommand::ownedRefs_.
    QList<std::shared_ptr<IBaseItem>> ownedRefs_;
};

// ============================================================================
// MoveItemsByCommand - Перемещение элементов на заданное смещение
// ============================================================================
class MoveItemsByCommand : public QUndoCommand
{
public:
    MoveItemsByCommand(const QList<QGraphicsItem*>& items,
                       const QPointF& delta,
                       bool ignoreFirstRedo = false);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QPointF delta_;
    bool ignoreFirstRedo_;
};

// ============================================================================
// ScaleItemsByCommand - Масштабирование элементов относительно якоря
// ============================================================================
class ScaleItemsByCommand : public QUndoCommand
{
public:
    ScaleItemsByCommand(const QList<QGraphicsItem*>& items,
                        qreal factor,
                        const QPointF& anchor,
                        bool ignoreFirstRedo = false);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    qreal factor_;
    QPointF anchor_;
    bool ignoreFirstRedo_;
};

// ============================================================================
// RotateItemsByCommand - Вращение элементов относительно якоря
// ============================================================================
class RotateItemsByCommand : public QUndoCommand
{
public:
    RotateItemsByCommand(const QList<QGraphicsItem*>& items,
                         qreal delta,
                         const QPointF& anchor,
                         bool ignoreFirstRedo = false);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    qreal delta_;
    QPointF anchor_;
    bool ignoreFirstRedo_;
};

// ============================================================================
// NormalizeItemsCommand - Нормализация размеров элементов
// ============================================================================
class NormalizeItemsCommand : public QUndoCommand
{
public:
    NormalizeItemsCommand(const QList<QGraphicsItem*>& items,
                          const QList<qreal>& scaleFactors);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QList<qreal> scaleFactors_;
    QList<qreal> oldScaleFactors_;
};

// ============================================================================
// FlipItemsCommand - Отражение элементов (горизонтальное/вертикальное)
// ============================================================================
class FlipItemsCommand : public QUndoCommand
{
public:
    FlipItemsCommand(const QList<QGraphicsItem*>& items,
                     const QPointF& anchor,
                     bool vertical);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QPointF anchor_;
    bool vertical_;
};

// ============================================================================
// ResetScaleCommand - Сброс масштаба элементов к 1.0
// ============================================================================
class ResetScaleCommand : public QUndoCommand
{
public:
    // anchor is a shared scene-space point (typically the selection's
    // bounding-box center) all items reset around, so a multi-item
    // selection reverts as one cohesive group instead of each item
    // spinning/scaling in place around its own center - matching how
    // ScaleItemsByCommand/RotateItemsByCommand apply a group transform.
    ResetScaleCommand(const QList<QGraphicsItem*>& items, const QPointF& anchor);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QPointF anchor_;
    QList<qreal> oldScaleFactors_;
};

// ============================================================================
// ResetRotationCommand - Сброс вращения элементов к 0
// ============================================================================
class ResetRotationCommand : public QUndoCommand
{
public:
    // See ResetScaleCommand::ResetScaleCommand() for why anchor is needed.
    ResetRotationCommand(const QList<QGraphicsItem*>& items,
                         const QPointF& anchor);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QPointF anchor_;
    QList<qreal> oldRotations_;
};

// ============================================================================
// ResetFlipCommand - Сброс отражения элементов
// ============================================================================
class ResetFlipCommand : public QUndoCommand
{
public:
    // See ResetScaleCommand::ResetScaleCommand() for why anchor is needed.
    ResetFlipCommand(const QList<QGraphicsItem*>& items, const QPointF& anchor);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QPointF anchor_;
    QList<qreal> oldFlips_;
};

// ============================================================================
// ResetCropCommand - Сброс кропа элементов
// ============================================================================
class ResetCropCommand : public QUndoCommand
{
public:
    explicit ResetCropCommand(const QList<IBaseItem*>& items);

    void redo() override;
    void undo() override;

private:
    QList<PixmapItem*> items_;
    QList<QRectF> oldCrops_;
};

// ============================================================================
// ResetTransformsCommand - Сброс всех трансформаций элементов
// ============================================================================
class ResetTransformsCommand : public QUndoCommand
{
public:
    // See ResetScaleCommand::ResetScaleCommand() (commands.h) for why
    // anchor is needed: a shared scene-space point (the selection's
    // bounding-box center) so a multi-item selection resets as one
    // cohesive group instead of each item scaling/rotating/flipping in
    // place around its own center.
    ResetTransformsCommand(const QList<IBaseItem*>& items,
                           const QPointF& anchor);

    void redo() override;
    void undo() override;

private:
    struct TransformValues
    {
        qreal scale;
        qreal rotation;
        qreal flip;
        QRectF crop;
        bool hasCrop;
    };

    QList<IBaseItem*> items_;
    QPointF anchor_;
    QList<TransformValues> oldValues_;
};

// ============================================================================
// ArrangeItemsCommand - Упорядочивание элементов по позициям
// ============================================================================
class ArrangeItemsCommand : public QUndoCommand
{
public:
    ArrangeItemsCommand(CanvasScene* scene,
                        const QList<QGraphicsItem*>& items,
                        const QList<QPointF>& positions);

    void redo() override;
    void undo() override;

private:
    CanvasScene* scene_;
    QList<QGraphicsItem*> items_;
    QList<QPointF> positions_;
    QList<QPointF> oldPositions_;
};

// ============================================================================
// ChangeOpacityCommand - Изменение прозрачности элементов
// ============================================================================
class ChangeOpacityCommand : public QUndoCommand
{
public:
    ChangeOpacityCommand(const QList<QGraphicsItem*>& items,
                         qreal opacity,
                         bool ignoreFirstRedo = false);
    void redo() override;
    void undo() override;
    void setOpacity(qreal opacity) { opacity_ = opacity; }
    void setIgnoreFirstRedo(bool value) { ignoreFirstRedo_ = value; }

private:
    QList<QGraphicsItem*> items_;
    qreal opacity_;
    QList<qreal> oldOpacities_;
    bool ignoreFirstRedo_;
};

// ============================================================================
// CropItemCommand - Применение кропа к элементу
// ============================================================================
class CropItemCommand : public QUndoCommand
{
public:
    CropItemCommand(PixmapItem* item, const QRectF& crop);

    void redo() override;
    void undo() override;

private:
    PixmapItem* item_;
    QRectF crop_;
    QRectF oldCrop_;
};

// ============================================================================
// ChangeTextCommand - Изменение текста текстового элемента
// ============================================================================
class ChangeTextCommand : public QUndoCommand
{
public:
    // Html, not plain text: one commit-on-exit diff carries typing and
    // any formatting/fill changes made by the text toolbar in the same
    // edit session.
    ChangeTextCommand(TextItem* item,
                      const QString& newHtml,
                      const QString& oldHtml,
                      const QColor& newFillColor,
                      const QColor& oldFillColor);

    void redo() override;
    void undo() override;

private:
    TextItem* item_;
    QString newHtml_;
    QString oldHtml_;
    QColor newFillColor_;
    QColor oldFillColor_;
};

// ============================================================================
// ToggleGrayscaleCommand - Переключение режима оттенков серого для изображений
// ============================================================================
class ToggleGrayscaleCommand : public QUndoCommand
{
public:
    // Each item independently inverts its own current grayscale state -
    // no shared target value, so a mixed selection (some grayscale, some
    // not) does something sensible instead of forcing everything to one
    // checkbox's state. Self-inverse, like FlipItemsCommand: undo() is
    // just redo() again.
    explicit ToggleGrayscaleCommand(const QList<PixmapItem*>& items);

    void redo() override;
    void undo() override;

private:
    QList<PixmapItem*> items_;
};

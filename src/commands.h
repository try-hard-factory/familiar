#pragma once

#include <QUndoCommand>
#include <QPointF>
#include <QList>
#include <QGraphicsItem>
#include <QRectF>

class CanvasScene;
class IBaseItem;
class PixmapItem;

// ============================================================================
// InsertItemsCommand - Вставка элементов на сцену
// ============================================================================
class InsertItemsCommand : public QUndoCommand
{
public:
    InsertItemsCommand(CanvasScene* scene,
                       const QList<IBaseItem*>& items,
                       const QPointF& position = QPointF(),
                       bool ignoreFirstRedo = false);

    void redo() override;
    void undo() override;

private:
    CanvasScene* scene_;
    QList<IBaseItem*> items_;
    QPointF position_;
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
    explicit ResetScaleCommand(const QList<QGraphicsItem*>& items);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QList<qreal> oldScaleFactors_;
};

// ============================================================================
// ResetRotationCommand - Сброс вращения элементов к 0
// ============================================================================
class ResetRotationCommand : public QUndoCommand
{
public:
    explicit ResetRotationCommand(const QList<QGraphicsItem*>& items);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QList<qreal> oldRotations_;
};

// ============================================================================
// ResetFlipCommand - Сброс отражения элементов
// ============================================================================
class ResetFlipCommand : public QUndoCommand
{
public:
    explicit ResetFlipCommand(const QList<QGraphicsItem*>& items);

    void redo() override;
    void undo() override;

private:
    QList<QGraphicsItem*> items_;
    QList<qreal> oldFlips_;
};

// ============================================================================
// ResetCropCommand - Сброс кропа элементов
// ============================================================================
class ResetCropCommand : public QUndoCommand
{
public:
    explicit ResetCropCommand(const QList<PixmapItem*>& items);

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
    explicit ResetTransformsCommand(const QList<IBaseItem*>& items);

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

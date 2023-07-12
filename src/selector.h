#pragma once

#include <utils/utils.h>
#include <QBrush>
#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include "canvasscene.h"
#include "canvasview.h"

template<typename T>
class BaseItemMixin : public T
{
public:
    explicit BaseItemMixin(T* parent = nullptr)
        : T(parent)
    {}

    void setScale(qreal value, const QPointF& anchor = QPointF(0, 0))
    {
        if (value <= 0) {
            return;
        }

        qDebug() << "Setting scale to" << value;
        this->prepareGeometryChange();
        QPointF prev = this->mapToScene(anchor);
        QGraphicsItem::setScale(value);
        QPointF diff = this->mapToScene(anchor) - prev;
        setPos(this->pos() - diff);
    }

    void setZValue(qreal value)
    {
        qDebug() << "Setting z-value to" << value;
        QGraphicsItem::setZValue(value);

        CanvasScene* scene = dynamic_cast<CanvasScene*>(this->scene());

        if (scene) {
            scene->max_z = qMax(scene->max_z, value);
            scene->min_z = qMin(scene->min_z, value);
        } else {
            qDebug() << "BaseItemMixin::setZValue Scene not found";
        }
    }

    void bringToFront()
    {
        CanvasScene* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (scene) {
            setZValue(scene->max_z + scene->Z_STEP);
        } else {
            qDebug() << "BaseItemMixin::bringToFront Scene not found";
        }
    }

    void setRotation(qreal value, const QPointF& anchor = QPointF(0, 0))
    {
        qDebug() << "Setting rotation to" << value;
        QGraphicsItem::setRotation(std::fmod(value, 360.0));
        QPointF prev = this->mapToScene(anchor);
        QPointF diff = this->mapToScene(anchor) - prev;
        setPos(this->pos() - diff);
    }

    qreal flip()
    {
        // We use the transformation matrix only for flipping, so checking
        // the x scale is enough
        return this->transform().m11();
    }

    void doFlip(bool vertical = false, const QPointF& anchor = QPointF(0, 0))
    {
        setTransform(QTransform::fromScale(-flip(), 1));
        if (vertical) {
            setRotation(this->rotation() + 180, anchor);
        }
    }

    QRectF boundingRectUnselected() const { return QGraphicsItem::boundingRect(); }

    qreal width() const { return boundingRectUnselected().width(); }

    qreal height() const { return boundingRectUnselected().height(); }

    QPointF center() const { return boundingRectUnselected().center(); }

    QPointF centerSceneCoords() const { return mapToScene(center()); }

    void on_selected_change() {}
};


template<typename Mixin, typename T>
class SelectableMixin : public BaseItemMixin<T>
{
public:
    struct FlipBounds
    {
        QRectF rect;
        bool flip_v;
    };

    explicit SelectableMixin(QGraphicsRectItem* parent = nullptr)
        : BaseItemMixin<T>(parent)
    {
        this->setAcceptHoverEvents(true);
        this->setFlag(QGraphicsItem::ItemIsMovable, true);
        this->setFlag(QGraphicsItem::ItemIsSelectable, true);
        resetActions();
        viewport_scale = 1;
        is_editable = false;
    }

    void resetActions()
    {
        scaleActive = false;
        rotateActive = false;
        flipActive = false;
    }

    bool isActionActive() const { return scaleActive || rotateActive || flipActive; }

    qreal fixed_length_for_viewport(qreal value)
    {
        qreal viewScale = 1.0;

        if (this->scene()) {
            QList<QGraphicsView*> views = this->scene()->views();
            if (!views.isEmpty()) {
                CanvasView* view = dynamic_cast<CanvasView*>(views.at(0));
                viewScale = view->get_scale();
            }
        }

        qreal scale = this->scale();
        return value / viewScale / scale;
    }

    qreal select_resize_size() const { return fixed_length_for_viewport(selectResizeSize); }
    qreal select_rotate_size() const { return fixed_length_for_viewport(selectRotateSize); }
    QRectF handleFreeCenterRect() const
    {
        qreal size = fixed_length_for_viewport(selectFreeCenter);
        qreal x = this->center().x() - size / 2;
        qreal y = this->center().y() - size / 2;
        return QRectF(x, y, size, size);
    }

    void paintSelectable(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget = nullptr)
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        if (static_cast<Mixin*>(this)->hasSelectionOutline() == false) {
            return;
        }

        painter->save();

        QColor selectColor(Qt::yellow);
        selectColor.setAlpha(128);
        painter->setPen(QPen(selectColor, selectLineWidth));
        painter->drawRect(this->boundingRectUnselected());

        if (static_cast<Mixin*>(this)->hasSelectionHandles() == true) {
            painter->setPen(QPen(Qt::blue));
            painter->setBrush(Qt::blue);
            for (const QPointF& corner : corners) {
                painter->drawEllipse(corner, selectHandleSize / 2, selectHandleSize / 2);
            }
        }

        painter->restore();
    }

    QVector<QPointF> corners() const
    {
        return {this->boundingRectUnselected().topLeft(),
                this->boundingRectUnselected().topRight(),
                this->boundingRectUnselected().bottomRight(),
                this->boundingRectUnselected().bottomLeft()};
    }

    QVector<QPointF> corners_scene_coords() const
    {
        QVector<QPointF> corners = this->corners();
        QVector<QPointF> cornersScene;
        cornersScene.reserve(corners.size());

        std::transform(corners.begin(),
                       corners.end(),
                       std::back_inserter(cornersScene),
                       [this](const QPointF& corner) { return this->mapToScene(corner); });

        return cornersScene;
    }

    QPainterPath getScaleBounds(const QPointF& corner, int margin = 0) const
    {
        QPainterPath path;
        qreal size = select_resize_size();
        qreal x = corner.x() - size / 2 - margin;
        qreal y = corner.y() - size / 2 - margin;
        path.addRect(x, y, size + 2 * margin, size + 2 * margin);

        return path;
    }

    QRectF getRotateBounds(const QPointF& corner) const
    {
        QPainterPath path;
        //https://bugreports.qt.io/browse/QTBUG-57567
        auto d = get_corner_direction(corner);
        auto p1 = corner - d * select_resize_size() / 2;
        auto p2 = p1 + d * (select_resize_size() + select_rotate_size());

        path.addRect(get_rect_from_points(p1, p2));

        return path - getScaleBounds(corner, 0.001);
    }

    std::vector<FlipBounds> getFlipBounds()
    {
        qreal outer_margin = select_resize_size() / 2;
        qreal inner_margin = select_resize_size() / 2;
        QPointF origin = this->boundingRectUnselected().topLeft();

        std::vector<FlipBounds> flipBounds;
        flipBounds.reserve(4);

        // Top
        flipBounds.push_back({QRectF(origin.x() + inner_margin,
                                     origin.y() - outer_margin,
                                     this->width() - 2 * inner_margin,
                                     outer_margin + inner_margin),
                              true});

        // Bottom
        flipBounds.push_back({QRectF(origin.x() + inner_margin,
                                     origin.y() + this->height() - inner_margin,
                                     this->width() - 2 * inner_margin,
                                     outer_margin + inner_margin),
                              true});

        // Left
        flipBounds.push_back({QRectF(origin.x() - outer_margin,
                                     origin.y() + inner_margin,
                                     outer_margin + inner_margin,
                                     this->height() - 2 * inner_margin),
                              false});

        // Right
        flipBounds.push_back({QRectF(origin.x() + this->width() - inner_margin,
                                     origin.y() + inner_margin,
                                     outer_margin + inner_margin,
                                     this->height() - 2 * inner_margin),
                              false});

        return flipBounds;
    }

    QRectF boundingRect() const override
    {
        if (static_cast<Mixin*>(this)->hasSelectionOutline() == false) {
            return this->boundingRectUnselected();
        }

        auto margin = select_resize_size() / 2 + select_rotate_size();
        return this->boundingRectUnselected().marginsAdded(QMarginsF(margin, margin, margin, margin));
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        if (static_cast<Mixin*>(this)->hasSelectionOutline() == true) {
            auto margin = select_resize_size() / 2;
            auto rect = this->boundingRectUnselected().marginsAdded(
                QMarginsF(margin, margin, margin, margin));
            path.addRect(rect);

            for_each(std::begin(corners()), std::end(corners()), [&path](const QPointF& corner) {
                path.addPath(getRotateBounds(corner));
            });
        } else {
            path.addRect(this->boundingRectUnselected());
        }

        return path;
    }

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
        if (static_cast<Mixin*>(this)->hasSelectionHandles() == false) {
            return;
        }

        QPointF pos = event->pos();
        if (isInHandleFreeCenter(pos)) {
            this->setCursor(Qt::ArrowCursor);
            return;
        }

        for (const QPointF& corner : corners()) {
            if (isInScaleHandle(corner, pos)) {
                this->setCursor(getCornerScaleCursor(corner));
                return;
            } else if (isInRotateHandle(corner, pos)) {
                this->setCursor(Qt::CrossCursor); //
                return;
            }
        }

        for (const auto& edge : getFlipBounds()) {
            if (isInFlipHandle(edge.rect, pos)) {
                if (getEdgeFlipsVertically(edge)) {
                    this->setCursor(Qt::UpArrowCursor);
                } else {
                    this->setCursor(Qt::SizeHorCursor);
                }
                return;
            }
        }

        this->setCursor(Qt::ArrowCursor);
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
        if (static_cast<Mixin*>(this)->hasSelectionHandles() == false) {
            this->setCursor(Qt::ArrowCursor);
        }
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        eventStart = event->scenePos();
        CanvasView* view = dynamic_cast<CanvasView*>(this->scene()->views().at(0));
        view->reset_previous_transform(this);

        if (!this->isSelected()) {
            //User has just selected this item with this click; don't
            //activate any transformations yet
            QGraphicsItem::mousePressEvent(event);
            return;
        }

        if (isInHandleFreeCenter(event->pos())) {
            //This area should always trigger regular move operations,
            //even if it is covered by selection scale/flip/... handles.
            //This ensures that small items can always still be moved/edited.
            QGraphicsItem::mousePressEvent(event);
            return;
        }

        if (event->button() == Qt::LeftButton
            && static_cast<Mixin*>(this)->hasSelectionHandles() == true) {
            for (auto& corner : corners()) {
                //Check if we are in one of the corner's scale areas
                if (getScaleBounds(corner).contains(event->pos())) {
                    scaleActive = true;
                    eventDirection = get_direction_from_center(event->scenePos());
                    eventAnchor = mapToScene(get_scale_anchor(corner));
                    for (auto& item : static_cast<Mixin*>(this)->selectionActionItems()) {
                        // need casting here
                        item->scaleOrigFactor = item->scale();
                    }
                    event->accept();
                    return;
                }

                //Check if we are in one of the corner's rotate areas
                if (getRotateBounds(corner).contains(event->pos())) {
                    rotateActive = true;
                    eventAnchor = this->centerSceneCoords();
                    rotateStartAngle = getRotateAngle(event->scenePos());
                    for (auto& item : static_cast<Mixin*>(this)->selectionActionItems()) {
                        // need casting here
                        item->rotateOrigDegrees = item->scale();
                    }
                    event->accept();
                    return;
                }

                //Check if we are in one of the flip edges:
                for (const auto& edge : getFlipBounds()) {
                    if (edge.rect.contains(event->pos())) {
                        flipActive = true;
                        event->accept();
                        //undo stack logic
                        return;
                    }
                }
            }
        }
        QGraphicsItem::mousePressEvent(event);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        QPointF pos = event->scenePos();
        if ((pos - eventStart).manhattanLength() > 5) {
            // Reset previous transform when movement exceeds threshold
            CanvasView* view = dynamic_cast<CanvasView*>(this->scene()->views().at(0));
            view->reset_previous_transform(nullptr);
        }

        if (scaleActive) {
            qreal factor = getScaleFactor(event);
            for (auto& item : static_cast<Mixin*>(this)->selectionActionItems()) {
                item->setScale(scaleOrigFactor * factor, item->mapFromscene(eventAnchor));
            }
            event->accept();
            return;
        } else if (rotateActive) {
            auto snap = (event->modifiers() == Qt::ShiftModifier
                         || event->modifiers() == Qt::ControlModifier);

            qreal delta = getRotateDelta(event->scenePos(), snap);
            for (auto& item : static_cast<Mixin*>(this)->selectionActionItems()) {
                // need casting here
                item->setRotation(rotateOrigDegrees + delta * item->flip(),
                                  item->mapFromScene(eventAnchor));
            }
            event->accept();
            return;
        } else if (flipActive) {
            // Already flipped on mouse press, accept event to prevent item move
            event->accept();
            return;
        }

        QGraphicsItem::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (scaleActive) {
            if (getScaleFactor(event) != 1) {
                // undo stack logic
            }
            event->accept();
            resetActions();
            return;
        } else if (rotateActive) {
            CanvasScene* scene = dynamic_cast<CanvasScene*>(this->scene());
            scene->onSelectionChange(); // or emit selectionChange()
            qreal delta = getRotateDelta(event->scenePos());
            if (delta != 0) {
                // undo stack logic
            }
            event->accept();
            resetActions();
            return;
        } else if (flipActive) {
            for (const auto& edge : getFlipBounds()) {
                if (isInFlipHandle(edge.rect, event->pos())) {
                    // Already flipped on mouse press, accept event to prevent item move
                    event->accept();
                    resetActions();
                    return;
                }
            }
        }

        resetActions();
        QGraphicsItem::mouseReleaseEvent(event);
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        paintSelectable(painter, option, widget);
    }

public slots:
    void on_view_scale_change() { this->prepareGeometryChange(); }
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value) override
    {
        if (change == QGraphicsItem::ItemSelectedChange) {
            this->prepareGeometryChange();
            this->on_selected_change();// or emit?
        }
        return QGraphicsItem::itemChange(change, value);
    }



public:
    bool isInHandleFreeCenter(const QPointF& pos) const
    {
        QRectF rect = handleFreeCenterRect();
        return rect.contains(pos);
    }

    bool isInScaleHandle(const QPointF& corner, const QPointF& pos) const
    {
        QRectF rect = getScaleBounds(corner);
        return rect.contains(pos);
    }

    bool isInRotateHandle(const QPointF& corner, const QPointF& pos) const
    {
        QRectF rect = getRotateBounds(corner);
        return rect.contains(pos);
    }

    bool isInFlipHandle(const QRectF& rect, const QPointF& pos) const { return rect.contains(pos); }

    QPointF get_scale_anchor(const QPointF& corner) const
    {
        auto origin = this->boundingRectUnselected().topLeft();
        return QPointF(this->width() - corner.x() + 2 * origin.x(),
                       this->height() - corner.y() + 2 * origin.y());
    }

    QPointF get_corner_direction(const QPointF& corner) const
    {
        //Get the direction facing away from the center, e.g. the direction
        //in which the scale for this corner increases.
        return QPointF((corner.x() > this->center().x()) ? 1 : -1, (corner.y() > this->center().y()) ? 1 : -1);
    }

    QPointF get_direction_from_center(const QPointF& pos) const
    {
        auto diff = pos - this->centerSceneCoords();
        qreal length = std::sqrt(QPointF::dotProduct(diff, diff));
        return diff / length;
    }
    qreal getRotateAngle(const QPointF& pos)
    {
        QPointF diff = pos - eventAnchor;
        return -std::atan2(diff.x(), diff.y()) * 180 / M_PI;
    }
    QCursor getCornerScaleCursor(const QPointF& corner) const
    {
        // Implement the desired cursor shape based on the corner position
        // and rotation of the item
        return Qt::SizeFDiagCursor;
    }

    Qt::CursorShape getCornerScaleCursor(const QPointF& corner)
    {
        bool isTopLeftOrBottomRight = (corner == this->boundingRectUnselected().topLeft()
                                       || corner == this->boundingRectUnselected().bottomRight());
        return getDiagCursor(isTopLeftOrBottomRight, this->rotation(), this->flip());
    }

    Qt::CursorShape getDiagCursor(bool isTopLeftOrBottomRight)
    {
        auto rotation = std::fmod(this->rotation(), 180);
        bool flipped = (this->flip() == -1);

        if (isTopLeftOrBottomRight) {
            if (22.5 < rotation && rotation < 67.5) {
                return Qt::SizeVerCursor;
            } else if (67.5 < rotation && rotation < 112.5) {
                return (flipped ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
            } else if (112.5 < rotation && rotation < 157.5) {
                return Qt::SizeHorCursor;
            } else {
                return (flipped ? Qt::SizeBDiagCursor : Qt::SizeFDiagCursor);
            }
        } else {
            if (22.5 < rotation && rotation < 67.5) {
                return Qt::SizeHorCursor;
            } else if (67.5 < rotation && rotation < 112.5) {
                return (flipped ? Qt::SizeBDiagCursor : Qt::SizeFDiagCursor);
            } else if (112.5 < rotation && rotation < 157.5) {
                return Qt::SizeVerCursor;
            } else {
                return (flipped ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
            }
        }
    }

    bool getEdgeFlipsVertically(const FlipBounds& edge) const
    {
        if ((this->rotation() > 45 && this->rotation() < 135) || (this->rotation() > 225 && this->rotation() < 315)) {
            return !edge.flip_v;
        } else {
            return edge.flip_v;
        }
    }

    qreal getScaleFactor(QGraphicsSceneMouseEvent* event) const
    {
        qreal imgSize = qSqrt(this->width() * this->width() + this->height() * this->height());
        QPointF p = event->scenePos() - eventStart;
        QPointF direction = eventDirection;
        qreal delta = QPointF::dotProduct(direction, p) / imgSize;
        return (scaleOrigFactor + delta) / scaleOrigFactor;
    }

    QPointF getScaleAnchor(const QPointF& corner)
    {
        auto origin = this->boundingRectUnselected().topLeft();
        return QPointF(this->width() - corner.x() + 2 * origin.x(),
                       this->height() - corner.y() + 2 * origin.y());
    }

    qreal getRotateDelta(const QPointF& pos, bool snap = false) const
    {
        auto delta = getRotateAngle(pos) - rotateStartAngle;
        if (snap) {
            auto target = roundTo(rotateOrigDegrees + delta, 15);
            delta = target - rotateOrigDegrees;
        }

        return delta;
    }


private:
    qreal selectLineWidth{4};
    qreal selectHandleSize{15};
    qreal selectResizeSize{20};
    qreal selectRotateSize{10};
    qreal selectFreeCenter{20};

    QPointF eventStart;
    QPointF eventDirection;
    qreal scaleOrigFactor;
    qreal rotateOrigDegrees;
    QPointF eventAnchor;
    qreal rotateStartAngle;
    bool scaleActive{false};
    bool rotateActive{false};
    bool flipActive{false};
    int viewport_scale{1};
    bool is_editable{false};

    //QVector<QPointF> corners;
    QVector<QRectF> flipBounds;
};


class MultiSelectItem : public SelectableMixin<MultiSelectItem, QGraphicsRectItem> {
public:
    virtual bool has_selection_handles() {
        // std::cout << "MultiSelectItem::has_selection_handles" << '\n';
        return true;
    }
};

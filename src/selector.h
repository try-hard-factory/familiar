#pragma once

#include "canvasscene.h"
#include "canvasview.h"
#include "commands.h"
#include "core/settings.h"
#include "core/settingshandler.h"
#include <utils/utils.h>
#include <QBrush>
#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <qnamespace.h>

class IBaseItem
{
public:
    virtual ~IBaseItem() = default;
    virtual IBaseItem* create_copy() = 0;
    virtual bool is_image() const = 0;
    virtual std::string get_type() const = 0;
    virtual bool is_editable() = 0;
    virtual void enter_crop_mode() = 0;
    virtual bool is_action_active() const = 0;
    virtual QVector<QPointF> corners_scene_coords() const = 0;
    virtual void bring_to_front() = 0;
    virtual void do_flip(bool vertical = false,
                         const QPointF& anchor = QPointF(0, 0))
        = 0;
    virtual void set_scale(qreal value, const QPointF& anchor = QPointF(0, 0))
        = 0;
    virtual void set_rotation(qreal value,
                             const QPointF& anchor = QPointF(0, 0))
        = 0;
    virtual QPointF center() const = 0;
    virtual qreal flip() const = 0;
};

template<typename T>
class BaseItemMixin : public T, public IBaseItem
{
public:
    explicit BaseItemMixin(T* parent = nullptr)
        : T(parent)
    {}

    void set_scale(qreal value, const QPointF& anchor = QPointF(0, 0)) override
    {
        if (value <= 0) {
            return;
        }

        qDebug() << "Setting scale to" << value;
        this->prepareGeometryChange();
        QPointF prev = this->mapToScene(anchor);
        QGraphicsItem::setScale(value);
        QPointF diff = this->mapToScene(anchor) - prev;
        this->setPos(this->pos() - diff);
    }

    void setZValue(qreal value)
    {
        qDebug() << "Setting z-value to" << value;
        QGraphicsItem::setZValue(value);

        auto* scene = dynamic_cast<CanvasScene*>(this->scene());

        if (scene) {
            scene->max_z = qMax(scene->max_z, value);
            scene->min_z = qMin(scene->min_z, value);
        } else {
            qDebug() << "BaseItemMixin::setZValue Scene not found";
        }
    }

    virtual void bring_to_front() override
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (scene) {
            setZValue(scene->max_z + scene->Z_STEP);
        } else {
            qDebug() << "BaseItemMixin::bring_to_front Scene not found";
        }
    }

    void set_rotation(qreal value, const QPointF& anchor = QPointF(0, 0)) override
    {
        qDebug() << "Setting rotation to" << value;
        QPointF prev = this->mapToScene(anchor);
        QGraphicsItem::setRotation(std::fmod(value, 360.0));
        QPointF diff = this->mapToScene(anchor) - prev;
        this->setPos(this->pos() - diff);
    }

    virtual qreal flip() const override
    {
        // We use the transformation matrix only for flipping, so checking
        // the x scale is enough
        return this->transform().m11();
    }

    void do_flip(bool vertical = false,
                const QPointF& anchor = QPointF(0, 0)) override
    {
        QPointF prev = this->mapToScene(anchor);
        this->setTransform(QTransform::fromScale(-flip(), 1));
        if (vertical) {
            this->set_rotation(this->rotation() + 180, anchor);
        }
        QPointF diff = this->mapToScene(anchor) - prev;
        this->setPos(this->pos() - diff);
    }

    virtual QRectF bounding_rect_unselected() const
    {
        return T::boundingRect();
    }

    qreal width() const { return bounding_rect_unselected().width(); }

    qreal height() const { return bounding_rect_unselected().height(); }

    QPointF center() const override { return bounding_rect_unselected().center(); }

    QPointF center_scene_coords() const { return this->mapToScene(center()); }

    void set_cursor(Qt::CursorShape c)
    {
        Q_ASSERT_X(false, "BaseItemMixin::set_cursor", "Should not be called");
    }

    void unset_cursor()
    {
        Q_ASSERT_X(false, "BaseItemMixin::unset_cursor", "Should not be called");
    }
    QColor sample_color_at(const QPointF& pos)
    {
        Q_ASSERT_X(false,
                   "BaseItemMixin::sample_color_at",
                   "Should not be called");
    }
    // void on_selected_change() {}
};


template<typename Mixin, typename T>
class SelectableMixin : public BaseItemMixin<T>
{
    qreal selectLineWidth_{4};
    qreal selectHandleSize_{15};
    qreal selectResizeSize_{20};
    qreal selectRotateSize_{10};
    qreal selectFreeCenter_{20};

    QPointF eventStart_;
    QPointF eventDirection_;
    qreal scaleOrigFactor_;
    qreal rotateOrigDegrees_;
    QPointF eventAnchor_;
    qreal rotateStartAngle_;
    int viewport_scale_{1};
    bool is_editable_{false};

    //QVector<QPointF> corners;
    QVector<QRectF> flipBounds;

public:
    struct FlipBounds
    {
        QRectF rect;
        bool flip_v;
    };

    enum EItemMode {
        kNone = 0,
        kScaleMode = 1,
        kRotateMode = 2,
        kFlipMpde = 3,
    };

    EItemMode active_mode_{kNone};

    explicit SelectableMixin(T* parent = nullptr)
        : BaseItemMixin<T>(parent)
    {}

    void init_selectable()
    {
        this->setAcceptHoverEvents(true);
        this->setFlag(QGraphicsItem::ItemIsMovable, true);
        this->setFlag(QGraphicsItem::ItemIsSelectable, true);
        viewport_scale_ = 1;
        is_editable_ = false;
    }

    qreal fixed_length_for_viewport(qreal value) const
    {
        qreal viewScale = 1.0;

        if (this->scene()) {
            QList<QGraphicsView*> views = this->scene()->views();
            if (!views.isEmpty()) {
                CanvasView* view = dynamic_cast<CanvasView*>(views.at(0));
                viewScale = view->get_scale();
            }
        }

        return value / viewScale / this->scale();
    }

    qreal select_resize_size() const
    {
        return fixed_length_for_viewport(selectResizeSize_);
    }

    qreal select_rotate_size() const
    {
        return fixed_length_for_viewport(selectRotateSize_);
    }

    QRectF select_handle_free_center() const
    {
        qreal size = fixed_length_for_viewport(selectFreeCenter_);
        qreal x = this->center().x() - size / 2;
        qreal y = this->center().y() - size / 2;
        return QRectF(x, y, size, size);
    }

    void draw_debug_shape(
        QPainter* painter, const QRectF& shape, int r, int g, int b) const
    {
        QColor color(r, g, b, 50);
        painter->fillRect(shape, color);
    }

    void draw_debug_shape(
        QPainter* painter, const QPainterPath& shape, int r, int g, int b) const
    {
        QColor color(r, g, b, 50);
        painter->fillPath(shape, color);
    }

    void paint_debug(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget)
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        const auto& args = CommandlineArgs::instance();

        if (args.debugShapes()) {
            draw_debug_shape(painter, this->shape(), 255, 0, 0);
        }
        if (args.debugBoundingRects()) {
            draw_debug_shape(painter, this->boundingRect(), 0, 255, 0);
        }
        if (args.debugHandles()
            && static_cast<Mixin*>(this)->has_selection_handles()) {
            for (const QPointF& corner : corners()) {
                draw_debug_shape(painter, get_scale_bounds(corner), 0, 0, 255);
                draw_debug_shape(painter, get_rotate_bounds(corner), 0, 255, 255);
            }
            for (const auto& edge : get_flip_bounds()) {
                draw_debug_shape(painter, edge.rect, 255, 255, 0);
            }
            draw_debug_shape(painter, select_handle_free_center(), 255, 0, 255);
        }
    }

    void paint_selectable(QPainter* painter,
                          const QStyleOptionGraphicsItem* option,
                          QWidget* widget = nullptr)
    {
        paint_debug(painter, option, widget);

        if (static_cast<Mixin*>(this)->has_selection_outline() == false) {
            return;
        }

        painter->save();

        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        QColor selectColor = colorPreset[EPresetsColorIdx::kSelectionColor];

        QPen pen(selectColor);
        pen.setWidth(selectLineWidth_);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(QBrush());

        // Draw the main selection rectangle
        painter->drawRect(this->bounding_rect_unselected());

        // If it's a single selection, draw the handles:
        if (static_cast<Mixin*>(this)->has_selection_handles() == true) {
            pen.setWidth(selectHandleSize_);
            pen.setCapStyle(Qt::RoundCap);
            painter->setPen(pen);
            for (const QPointF& corner : corners()) {
                painter->drawPoint(corner);
            }
        }

        painter->restore();
    }

    QVector<QPointF> corners() const
    {
        return {this->bounding_rect_unselected().topLeft(),
                this->bounding_rect_unselected().topRight(),
                this->bounding_rect_unselected().bottomRight(),
                this->bounding_rect_unselected().bottomLeft()};
    }

    QVector<QPointF> corners_scene_coords() const override
    {
        QVector<QPointF> corners = this->corners();
        QVector<QPointF> cornersScene;
        cornersScene.reserve(corners.size());

        std::transform(corners.begin(),
                       corners.end(),
                       std::back_inserter(cornersScene),
                       [this](const QPointF& corner) {
                           return this->mapToScene(corner);
                       });

        return cornersScene;
    }

    // Whether this item is currently mid scale/rotate/flip drag.
    bool is_action_active() const override { return active_mode_ != kNone; }

    QPainterPath get_scale_bounds(const QPointF& corner, int margin = 0) const
    {
        QPainterPath path;
        qreal size = select_resize_size();
        qreal x = corner.x() - size / 2 - margin;
        qreal y = corner.y() - size / 2 - margin;
        path.addRect(x, y, size + 2 * margin, size + 2 * margin);

        return path;
    }

    // The interactable shape of the rotation area. It sits around the
    // scale area like an L shape, e.g. for the bottom right corner:
    //    │
    //   ┌┴┬─┐
    //  ─┤S│R│
    //   ├─┘ │
    //   │R R│
    //   └───┘
    QPainterPath get_rotate_bounds(const QPointF& corner) const
    {
        QPainterPath path;
        auto d = get_corner_direction(corner);
        auto p1 = corner - d * select_resize_size() / 2;
        auto p2 = p1 + d * (select_resize_size() + select_rotate_size());

        path.addRect(get_rect_from_points(p1, p2));

        return path - get_scale_bounds(corner, 0.001);
    }


    // The interactactable shape of the flip handles.
    // These stretch around the edge of the item filling the areas
    // between the scale handles, e.g. for the bottom right corner:

    //    │F│
    //  ──┼─┼─┐
    //  FF│S│R│
    //  ──┼─┘ │
    //    │R R│
    //    └───┘

    std::vector<FlipBounds> get_flip_bounds()
    {
        qreal outer_margin = select_resize_size() / 2;
        qreal inner_margin = select_resize_size() / 2;
        QPointF origin = this->bounding_rect_unselected().topLeft();

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
        // TODOLATER: has_selection_outline check this func
        if (static_cast<const Mixin*>(this)->has_selection_outline() == false) {
            return this->bounding_rect_unselected();
        }

        auto margin = select_resize_size() / 2 + select_rotate_size();
        return this->bounding_rect_unselected().marginsAdded(
            QMarginsF(margin, margin, margin, margin));
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        if (static_cast<const Mixin*>(this)->has_selection_outline() == true) {
            auto margin = select_resize_size() / 2;
            auto rect = this->bounding_rect_unselected().marginsAdded(
                QMarginsF(margin, margin, margin, margin));
            path.addRect(rect);

            for (const QPointF& corner : corners()) {
                path.addPath(this->get_rotate_bounds(corner));
            }
        } else {
            path.addRect(this->bounding_rect_unselected());
        }

        return path;
    }

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
        if (static_cast<Mixin*>(this)->has_selection_handles() == false) {
            return;
        }

        QPointF pos = event->pos();
        if (isInHandleFreeCenter(pos)) {
            // this->setCursor(Qt::ArrowCursor);
            this->unsetCursor();
            return;
        }

        for (const QPointF& corner : corners()) {
            if (isInScaleHandle(corner, pos)) {
                this->setCursor(get_corner_scale_cursor(corner));
                return;
            } else if (isInRotateHandle(corner, pos)) {
                // TODOLATER: custom rotate icon
                this->setCursor(Qt::SizeAllCursor);
                return;
            }
        }

        for (const auto& edge : get_flip_bounds()) {
            if (isInFlipHandle(edge.rect, pos)) {
                // TODOLATER: custom flip icons
                if (get_edge_flips_v(edge)) {
                    this->setCursor(Qt::SizeVerCursor);
                } else {
                    this->setCursor(Qt::SizeHorCursor);
                }
                return;
            }
        }

        // this->setCursor(Qt::ArrowCursor);
        this->unsetCursor();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
        qDebug() << "hoverEnterEvent !!!!!!!!!!!!!!";
        if (static_cast<Mixin*>(this)->has_selection_handles() == false) {
            // this->setCursor(Qt::ArrowCursor);
            this->unsetCursor();
        }
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
    {
        this->unsetCursor();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        eventStart_ = event->scenePos();
        CanvasView* view = dynamic_cast<CanvasView*>(
            this->scene()->views().at(0));
        // TODOLATER: assert view?
        view->resetPreviousTransform(this);

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
            && static_cast<Mixin*>(this)->has_selection_handles() == true) {
            for (auto& corner : corners()) {
                //Check if we are in one of the corner's scale areas
                if (get_scale_bounds(corner).contains(event->pos())) {
                    active_mode_ = kScaleMode;
                    eventDirection_ = get_direction_from_center(
                        event->scenePos());
                    eventAnchor_ = this->mapToScene(get_scale_anchor(corner));
                    for (auto& item :
                         static_cast<Mixin*>(this)->selection_action_items()) {
                        // need casting here
                        auto* item_cast = dynamic_cast<SelectableMixin*>(item);
                        item_cast->scaleOrigFactor_ = item_cast->scale();
                    }
                    event->accept();
                    return;
                }

                //Check if we are in one of the corner's rotate areas
                if (isInRotateHandle(corner, event->pos())) {
                    active_mode_ = kRotateMode;
                    eventAnchor_ = this->center_scene_coords();
                    rotateStartAngle_ = get_rotate_angle(event->scenePos());
                    for (auto& item :
                         static_cast<Mixin*>(this)->selection_action_items()) {
                        // need casting here
                        auto* item_cast = dynamic_cast<SelectableMixin*>(item);
                        item_cast->rotateOrigDegrees_ = item_cast->rotation();
                    }
                    event->accept();
                    return;
                }

                //Check if we are in one of the flip edges:
                for (const auto& edge : get_flip_bounds()) {
                    if (edge.rect.contains(event->pos())) {
                        active_mode_ = kFlipMpde;
                        event->accept();
                        //undo stack logic
                        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
                        // TODOLATER: interface
                        scene->undo_stack_->push(
                            new FlipItemsCommand(static_cast<Mixin*>(this)
                                                     ->selection_action_items(),
                                                 this->center_scene_coords(),
                                                 this->get_edge_flips_v(edge)));
                        return;
                    }
                }
            }
        }

        QGraphicsItem::mousePressEvent(event);
    }

    qreal get_scale_factor(QGraphicsSceneMouseEvent* event) const
    {
        qreal imgSize = qSqrt(this->width() * this->width()
                              + this->height() * this->height());
        QPointF p = event->scenePos() - eventStart_;
        QPointF direction = eventDirection_;
        qreal delta = QPointF::dotProduct(direction, p) / imgSize;
        return (scaleOrigFactor_ + delta) / scaleOrigFactor_;
    }

    QPointF get_scale_anchor(const QPointF& corner)
    {
        auto origin = this->bounding_rect_unselected().topLeft();
        return QPointF(this->width() - corner.x() + 2 * origin.x(),
                       this->height() - corner.y() + 2 * origin.y());
    }

    QPointF get_corner_direction(const QPointF& corner) const
    {
        //Get the direction facing away from the center, e.g. the direction
        //in which the scale for this corner increases.
        return QPointF((corner.x() > this->center().x()) ? 1 : -1,
                       (corner.y() > this->center().y()) ? 1 : -1);
    }

    QPointF get_direction_from_center(const QPointF& pos) const
    {
        auto diff = pos - this->center_scene_coords();
        qreal length = std::sqrt(QPointF::dotProduct(diff, diff));
        return diff / length;
    }

    qreal get_rotate_angle(const QPointF& pos) const
    {
        QPointF diff = pos - eventAnchor_;
        return -std::atan2(diff.x(), diff.y()) * 180 / M_PI;
    }

    qreal get_rotate_delta(const QPointF& pos, bool snap = false) const
    {
        auto delta = get_rotate_angle(pos) - rotateStartAngle_;
        if (snap) {
            auto target = roundTo(rotateOrigDegrees_ + delta, 15);
            delta = target - rotateOrigDegrees_;
        }

        return delta;
    }

    Qt::CursorShape get_corner_scale_cursor(const QPointF& corner)
    {
        bool isTopLeftOrBottomRight
            = (corner == this->bounding_rect_unselected().topLeft()
               || corner == this->bounding_rect_unselected().bottomRight());
        return get_diag_cursor(isTopLeftOrBottomRight);
    }

    Qt::CursorShape get_diag_cursor(bool isTopLeftOrBottomRight)
    {
        auto rotation = std::fmod(this->rotation(), 180);
        bool flipped = (this->flip() == -1);

        if (isTopLeftOrBottomRight) {
            if ((22.5 < rotation) && (rotation < 67.5)) {
                return Qt::SizeVerCursor;
            } else if ((67.5 < rotation) && (rotation < 112.5)) {
                return (flipped ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
            } else if ((112.5 < rotation) && (rotation < 157.5)) {
                return Qt::SizeHorCursor;
            } else {
                return (flipped ? Qt::SizeBDiagCursor : Qt::SizeFDiagCursor);
            }
        } else {
            if ((22.5 < rotation) && (rotation < 67.5)) {
                return Qt::SizeHorCursor;
            } else if ((67.5 < rotation) && (rotation < 112.5)) {
                return (flipped ? Qt::SizeBDiagCursor : Qt::SizeFDiagCursor);
            } else if ((112.5 < rotation) && (rotation < 157.5)) {
                return Qt::SizeVerCursor;
            } else {
                return (flipped ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
            }
        }
    }

    bool get_edge_flips_v(const FlipBounds& edge) const
    {
        if (((this->rotation() > 45) && (this->rotation() < 135))
            || ((this->rotation() > 225) && (this->rotation() < 315))) {
            return !edge.flip_v;
        } else {
            return edge.flip_v;
        }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        QPointF pos = event->scenePos();
        if ((pos - eventStart_).manhattanLength() > 5) {
            // Reset previous transform when movement exceeds threshold
            CanvasView* view = dynamic_cast<CanvasView*>(
                this->scene()->views().at(0));
            view->resetPreviousTransform(nullptr);
        }

        if (active_mode_ == kScaleMode) {
            qreal factor = get_scale_factor(event);
            for (auto& item :
                 static_cast<Mixin*>(this)->selection_action_items()) {
                auto* item_cast = dynamic_cast<SelectableMixin*>(item);
                item_cast->set_scale(item_cast->scaleOrigFactor_ * factor,
                                    item->mapFromScene(eventAnchor_));
            }
            event->accept();
            return;
        } else if (active_mode_ == kRotateMode) {
            auto snap = (event->modifiers()
                             == Qt::KeyboardModifier::ShiftModifier
                         || event->modifiers()
                                == Qt::KeyboardModifier::ControlModifier);

            qreal delta = get_rotate_delta(event->scenePos(), snap);
            for (auto& item :
                 static_cast<Mixin*>(this)->selection_action_items()) {
                // need casting here
                auto* item_cast = dynamic_cast<SelectableMixin*>(item);
                item_cast->set_rotation(item_cast->rotateOrigDegrees_
                                           + delta * item_cast->flip(),
                                       item->mapFromScene(eventAnchor_));
            }
            event->accept();
            return;
        } else if (active_mode_ == kFlipMpde) {
            // Already flipped on mouse press, accept event to prevent item move
            event->accept();
            return;
        }

        QGraphicsItem::mouseMoveEvent(event);
    }

    void resetActions() { active_mode_ = kNone; }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (active_mode_ == kScaleMode) {
            qreal factor = get_scale_factor(event);
            if (factor != 1) {
                // TODOLATER: static or dynamic? assert?
                auto* scene = dynamic_cast<CanvasScene*>(this->scene());
                scene->undo_stack_->push(
                    new ScaleItemsByCommand(static_cast<Mixin*>(this)
                                                ->selection_action_items(),
                                            factor,
                                            eventAnchor_,
                                            true));
            }
            event->accept();
            resetActions();
            return;
        } else if (active_mode_ == kRotateMode) {
            CanvasScene* scene = dynamic_cast<CanvasScene*>(this->scene());
            scene->on_selection_changed(); // or emit selectionChange()
            qreal delta = get_rotate_delta(event->scenePos());
            if (delta != 0) {
                scene->undo_stack_->push(
                    new RotateItemsByCommand(static_cast<Mixin*>(this)
                                                 ->selection_action_items(),
                                             delta,
                                             eventAnchor_,
                                             true));
            }
            event->accept();
            resetActions();
            return;
        } else if (active_mode_ == kFlipMpde) {
            for (const auto& edge : get_flip_bounds()) {
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
        paint_selectable(painter, option, widget);
    }

public slots:
    void on_view_scale_change() { this->prepareGeometryChange(); }

protected:
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                        const QVariant& value) override
    {
        if (change == QGraphicsItem::ItemSelectedChange) {
            this->prepareGeometryChange();
            static_cast<Mixin*>(this)->on_selected_change(value.toBool());
        }
        return QGraphicsItem::itemChange(change, value);
    }


public:
    bool isInHandleFreeCenter(const QPointF& pos) const
    {
        QRectF rect = select_handle_free_center();
        return rect.contains(pos);
    }

    bool isInScaleHandle(const QPointF& corner, const QPointF& pos) const
    {
        QPainterPath rect = get_scale_bounds(corner);
        return rect.contains(pos);
    }

    bool isInRotateHandle(const QPointF& corner, const QPointF& pos) const
    {
        QPainterPath rect = get_rotate_bounds(corner);
        return rect.contains(pos);
    }
    bool isInFlipHandle(const QRectF& rect, const QPointF& pos) const
    {
        return rect.contains(pos);
    }
};


class MultiSelectItem
    : public SelectableMixin<MultiSelectItem, QGraphicsRectItem>
{
public:
    MultiSelectItem(QGraphicsRectItem* parent = nullptr)
        : SelectableMixin<MultiSelectItem, QGraphicsRectItem>(parent)
    {
        this->init_selectable();
        qDebug() << "Initialized " << toString();
    }

    QString toString() const
    {
        return QString("MultiSelectItem %1 x %2")
            .arg(this->width())
            .arg(this->height());
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        this->paint_selectable(painter, option, widget);
    }

    virtual bool has_selection_outline() const { return true; }
    virtual bool has_selection_handles() { return true; }

    virtual QVector<QGraphicsItem*> selection_action_items()
    {
        if (this->scene()) {
            return this->scene()->selectedItems();
        }
        return {};
    }

    // Temporarily sends the multi-select rectangle behind the current
    // selection, e.g. so it doesn't intercept color sampling clicks.
    void lower_behind_selection()
    {
        QVector<QGraphicsItem*> items = selection_action_items();
        if (items.isEmpty()) {
            return;
        }

        qreal minZ = items.first()->zValue();
        for (auto* item : items) {
            minZ = qMin(minZ, item->zValue());
        }

        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        this->setZValue(minZ - scene->Z_STEP);
    }

    void fit_selection_area(const QRectF& rect)
    {
        if (this->width() != rect.width() || this->height() != rect.height()) {
            this->setRect(0, 0, rect.width(), rect.height());
        }
        if (this->pos() != rect.topLeft()) {
            this->setPos(rect.topLeft());
        }
        if (this->scale() != 1) {
            this->setScale(1);
        }
        if (this->rotation() != 0) {
            this->setRotation(0);
        }
        if (!this->isSelected()) {
            this->setSelected(true);
        }
        if (this->flip() == -1) {
            this->setTransform(QTransform::fromScale(1, 1));
        }
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton
            && event->modifiers() == Qt::ControlModifier) {
            // We still need to be able to select additional images
            // within/"under" the multi select rectangle, so let ctrl+click
            // events pass through
            event->ignore();
            return;
        }

        QGraphicsItem::mousePressEvent(event);
    }

public:
    IBaseItem* create_copy() override
    {
        Q_ASSERT_X(false,
                   "MultiSelectItem::create_copy",
                   "Should not be called");
        return nullptr;
    }
    bool is_editable() override
    {
        Q_ASSERT_X(false,
                   "MultiSelectItem::is_editable",
                   "Should not be called");
        return false;
    }
    void enter_crop_mode() override
    {
        Q_ASSERT_X(false,
                   "MultiSelectItem::enter_crop_mode",
                   "Should not be called");
    }
    bool is_image() const override
    {
        Q_ASSERT_X(false, "MultiSelectItem::is_image", "Should not be called");
        return false;
    }
    std::string get_type() const override
    {
        Q_ASSERT_X(false, "MultiSelectItem::get_type", "Should not be called");
        return {};
    }

    // No selection-driven bring-to-front logic for the multi-select
    // rectangle itself (Python's MultiSelectItem has no on_selected_change,
    // so BeeItemMixin's hasattr() guard silently skips it there).
    void on_selected_change(bool value) { Q_UNUSED(value) }

    QRectF boundingRect() const override
    {
        return QGraphicsRectItem::boundingRect();
    }
};

class RubberbandItem : public BaseItemMixin<QGraphicsRectItem>
{
public:
    RubberbandItem(QGraphicsRectItem* parent = nullptr)
        : BaseItemMixin<QGraphicsRectItem>(parent)
    {
        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        QColor fillColor = colorPreset[EPresetsColorIdx::kSelectionColor];
        fillColor.setAlpha(40);
        this->setBrush(QBrush(fillColor));

        pen.setWidth(1);
        pen.setCosmetic(true);
        this->setPen(pen);
    }

    QString toString() const
    {
        return QString("RubberbandItem %1 x %2")
            .arg(this->width())
            .arg(this->height());
    }

    // Updates itself to fit the two given points.
    void fit(const QPointF& point1, const QPointF& point2)
    {
        this->setRect(get_rect_from_points(point1, point2));
        qDebug() << "Updated rubberband " << toString();
    }

    IBaseItem* create_copy() override
    {
        Q_ASSERT_X(false, "RubberbandItem::create_copy", "Should not be called");
        return nullptr;
    }
    bool is_editable() override
    {
        Q_ASSERT_X(false, "RubberbandItem::is_editable", "Should not be called");
        return false;
    }
    void enter_crop_mode() override
    {
        Q_ASSERT_X(false,
                   "RubberbandItem::enter_crop_mode",
                   "Should not be called");
    }
    bool is_image() const override
    {
        Q_ASSERT_X(false, "RubberbandItem::is_image", "Should not be called");
        return false;
    }
    std::string get_type() const override
    {
        Q_ASSERT_X(false, "RubberbandItem::get_type", "Should not be called");
        return {};
    }
    // Doesn't go through SelectableMixin in Python either, so it has no
    // concept of an active scale/rotate/flip drag.
    bool is_action_active() const override { return false; }
    QVector<QPointF> corners_scene_coords() const override
    {
        Q_ASSERT_X(false,
                   "RubberbandItem::corners_scene_coords",
                   "Should not be called");
        return QVector<QPointF>();
    }

    QColor color{Qt::black};
    QPen pen{color};
};
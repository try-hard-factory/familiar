#pragma once

#include "canvasscene.h"
#include "canvasview.h"
#include "commands.h"
#include "core/settings.h"
#include "core/settingshandler.h"
#include <utils/utils.h>

#include "log/log.h"
#include <memory>
#include <QBrush>
#include <QCursor>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QUuid>
#include <QVariantMap>
#include <qnamespace.h>

class IBaseItem : public std::enable_shared_from_this<IBaseItem>
{
public:
    virtual ~IBaseItem() = default;

    // Multiple independent owners can end up referencing the same item
    // across its lifetime - e.g. the InsertItemsCommand that created it
    // and, later, a DeleteItemsCommand for the same item are both alive
    // on the undo stack at once, and CanvasScene itself holds a
    // reference while the item is attached. Each caller obtains its
    // share via this method instead of independently wrapping the raw
    // pointer, so they end up sharing one control block/refcount rather
    // than each thinking it's the sole owner (which would double-free).
    // The first call for a given object creates the control block; every
    // later call (from anyone) just shares it.
    std::shared_ptr<IBaseItem> acquireShared()
    {
        if (auto existing = weak_from_this().lock())
            return existing;
        return std::shared_ptr<IBaseItem>(this);
    }
    virtual IBaseItem* create_copy() = 0;
    virtual bool is_image() const = 0;
    virtual std::string get_type() const = 0;
    virtual bool is_editable() = 0;
    virtual void enter_crop_mode() = 0;
    virtual bool is_action_active() const = 0;
    virtual QVector<QPointF> corners_scene_coords() const = 0;
    virtual void bring_to_front() = 0;
    virtual void set_z_value(qreal value) = 0;
    virtual void do_flip(bool vertical = false,
                         const QPointF& anchor = QPointF(0, 0))
        = 0;
    virtual void set_scale(qreal value, const QPointF& anchor = QPointF(0, 0))
        = 0;
    virtual void set_rotation(qreal value, const QPointF& anchor = QPointF(0, 0))
        = 0;
    virtual QPointF center() const = 0;
    virtual qreal flip() const = 0;
    virtual QUuid uid() const = 0;
    virtual void set_uid(const QUuid& value) = 0;
    // Type-specific fields for manifest.json's "data" object (crop, text,
    // ...). Empty for items that structurally aren't saved (MultiSelectItem,
    // RubberbandItem, ErrorItem).
    virtual QVariantMap get_extra_save_data() const = 0;
    virtual void on_view_scale_change() = 0;
    // Transient per-drag state for scale/rotate handle interactions
    // (selection_action_items() can be heterogeneous - e.g. MultiSelectItem
    // driving a mix of PixmapItem/TextItem - so this can't be stashed via a
    // dynamic_cast to the initiating item's own concrete SelectableMixin
    // type, which only succeeds for items of that exact type).
    virtual qreal scale_orig_factor() const = 0;
    virtual void set_scale_orig_factor(qreal value) = 0;
    virtual qreal rotate_orig_degrees() const = 0;
    virtual void set_rotate_orig_degrees(qreal value) = 0;

    // Additional items that should participate in the SAME handle-driven
    // scale/rotate operation as this one, beyond itself - e.g. GroupItem
    // (moveitem.h) overrides this to return every one of its own
    // descendants recursively, so each gets its own independently-
    // anchored transform call too. Default: nothing extra (a plain item
    // has no descendants). Used by MultiSelectItem::selection_action_items()
    // below to expand a flat multi-selection when it includes a
    // GroupItem - without this, a group nested inside a multi-selection
    // (rather than being the sole selected item) would leave its own
    // un-selected members behind with only a crude position-cascade via
    // itemChange() (which never applies scale/rotation, only a plain
    // translate), visibly shearing/misaligning them relative to the rest
    // of the rotated/scaled selection (confirmed with Max). Not pure -
    // deliberately has a safe default body rather than requiring every
    // IBaseItem implementor to define it, since only GroupItem actually
    // needs non-trivial behavior here.
    virtual QList<QGraphicsItem*> nested_selection_action_items()
    {
        return {};
    }
};

template<typename T>
class BaseItemMixin : public T, public IBaseItem
{
    // See uid()/set_uid() below.
    QUuid uid_{QUuid::createUuid()};

public:
    explicit BaseItemMixin(T* parent = nullptr)
        : T(parent)
    {}

    void set_scale(qreal value, const QPointF& anchor = QPointF(0, 0)) override
    {
        if (value <= 0) {
            return;
        }

        FLOG_DEBUG(familiar::log::Ch::Items, "Setting scale to {}", value);
        this->prepareGeometryChange();
        QPointF prev = this->mapToScene(anchor);
        QGraphicsItem::setScale(value);
        QPointF diff = this->mapToScene(anchor) - prev;
        this->setPos(this->pos() - diff);
    }

    void set_z_value(qreal value) override
    {
        FLOG_DEBUG(familiar::log::Ch::Items, "Setting z-value to {}", value);
        QGraphicsItem::setZValue(value);

        auto* scene = dynamic_cast<CanvasScene*>(this->scene());

        if (scene) {
            scene->max_z = qMax(scene->max_z, value);
            scene->min_z = qMin(scene->min_z, value);
        } else {
            FLOG_DEBUG(familiar::log::Ch::Items,
                       "BaseItemMixin::setZValue Scene not found");
        }
    }

    virtual void bring_to_front() override
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (scene) {
            set_z_value(scene->max_z + scene->Z_STEP);
        } else {
            FLOG_DEBUG(familiar::log::Ch::Items,
                       "BaseItemMixin::bring_to_front Scene not found");
        }
    }

    void set_rotation(qreal value,
                      const QPointF& anchor = QPointF(0, 0)) override
    {
        FLOG_DEBUG(familiar::log::Ch::Items, "Setting rotation to {}", value);
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

    QPointF center() const override
    {
        return bounding_rect_unselected().center();
    }

    QPointF center_scene_coords() const { return this->mapToScene(center()); }

    // Permanent identity - see IBaseItem::uid(). Generated once here for
    // every item (including helper items like MultiSelectItem/
    // RubberbandItem that are never saved; a uid is harmless for those,
    // and giving every IBaseItem one unconditionally is simpler than
    // tracking which concrete types opt in).
    QUuid uid() const override { return uid_; }
    void set_uid(const QUuid& value) override { uid_ = value; }

    // Default for items that carry no type-specific save data
    // (MultiSelectItem, RubberbandItem, ErrorItem - the latter uses
    // original_uid instead, matching Python's BeeErrorItem).
    QVariantMap get_extra_save_data() const override { return {}; }

    qreal scale_orig_factor() const override
    {
        Q_ASSERT_X(false,
                   "BaseItemMixin::scale_orig_factor",
                   "Should not be called");
        return 1;
    }

    void set_scale_orig_factor(qreal value) override
    {
        Q_UNUSED(value)
        Q_ASSERT_X(false,
                   "BaseItemMixin::set_scale_orig_factor",
                   "Should not be called");
    }

    qreal rotate_orig_degrees() const override
    {
        Q_ASSERT_X(false,
                   "BaseItemMixin::rotate_orig_degrees",
                   "Should not be called");
        return 0;
    }

    void set_rotate_orig_degrees(qreal value) override
    {
        Q_UNUSED(value)
        Q_ASSERT_X(false,
                   "BaseItemMixin::set_rotate_orig_degrees",
                   "Should not be called");
    }

    // RubberbandItem is the only BaseItemMixin-direct user and never sets
    // ItemIsSelectable, so it structurally never appears in selectedItems()
    // and this is unreachable. SelectableMixin overrides this for every
    // selectable item (pixmap/text/multiselect) with the real
    // prepareGeometryChange() implementation.
    void on_view_scale_change() override
    {
        Q_ASSERT_X(false,
                   "BaseItemMixin::on_view_scale_change",
                   "Should not be called");
    }

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
    qreal selectLineWidth_{2};
    qreal selectHandleSize_{9};
    qreal selectEdgeHandleSize_{6};
    qreal selectResizeSize_{20};
    qreal selectRotateSize_{10};
    qreal selectFreeCenter_{20};

    QPointF eventStart_;
    QPointF eventDirection_;
    // Distance from the fixed anchor (the opposite corner/edge-midpoint,
    // mirrored across the center - see get_scale_anchor()) to the handle
    // being dragged, at scale 1. get_scale_factor() divides by this to
    // convert a mouse-movement distance into a scale delta; see its
    // comment for why this must differ between a corner drag (the
    // diagonal) and an edge drag (just that edge's own dimension).
    qreal scaleRefLength_{0};
    qreal scaleOrigFactor_;
    qreal rotateOrigDegrees_;
    QPointF eventAnchor_;
    qreal rotateStartAngle_;
    int viewport_scale_{1};
    bool is_editable_{false};

    // kFieldResizeMode drag state (edit-mode square handles only - see
    // ItemMixin::resize_field(), moveitem.h). eventAnchor_ above holds
    // the SCENE position of the fixed corner/edge, captured once at
    // press - re-mapped into LOCAL coordinates fresh on every
    // mouseMoveEvent (via mapFromScene()) rather than cached, since the
    // anchor's LOCAL coordinate is exactly the thing that changes as the
    // field is resized (e.g. the right edge's local x is always
    // "current width", not the width at press time).
    // Which side is fixed while the OTHER side follows the cursor - true
    // = the right/bottom edge is the anchor (dragging the left/top
    // handle), matching resize_field()'s own parameter names.
    bool fieldResizeAnchorRight_{false};
    bool fieldResizeAnchorBottom_{false};
    bool fieldResizeAffectsWidth_{false};
    bool fieldResizeAffectsHeight_{false};
    // Size at the moment the drag started, for the undo command pushed
    // in mouseReleaseEvent() (ResizeTextFieldCommand needs an "old"
    // value; unlike scale/rotation there's no separately-tracked
    // per-item "orig" accessor for this, since it's TextItem-only).
    QSizeF fieldResizeOrigSize_;

    //QVector<QPointF> corners;
    QVector<QRectF> flipBounds;

public:
    // Was FlipBounds/flip_v - these edge zones used to trigger a flip on
    // click; they now trigger the same uniform kScaleMode drag as the
    // corner handles (see mousePressEvent()), just anchored on the
    // opposite edge instead of the opposite corner. `vertical` still
    // marks whether this is a top/bottom edge (true) or left/right
    // (false) - used to pick the resize cursor in hoverMoveEvent().
    struct EdgeBounds
    {
        QRectF rect;
        bool vertical;
    };

    enum EItemMode {
        kNone = 0,
        kScaleMode = 1,
        kRotateMode = 2,
        // Edit-mode (square handle) drag - resizes the field itself
        // (ItemMixin::resize_field()) instead of uniformly scaling.
        kFieldResizeMode = 3,
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
            for (const auto& edge : get_edge_bounds()) {
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

        // "Peek": the multi-select bounding box fades out
        // while this window isn't active and the cursor isn't hovering
        // it either, so it doesn't stay visible while the user's
        // working in another window - see CanvasView::
        // updateSelectionVisibility(). Individual items' own selection
        // outlines are unaffected (see fades_with_window_focus()).
        qreal outlineOpacity = 1.0;
        if (static_cast<Mixin*>(this)->fades_with_window_focus()
            && this->scene()) {
            QList<QGraphicsView*> views = this->scene()->views();
            if (!views.isEmpty()) {
                if (CanvasView* view = dynamic_cast<CanvasView*>(views.at(0))) {
                    outlineOpacity = view->selectionOutlineOpacity();
                }
            }
        }
        if (outlineOpacity <= 0.0) {
            return;
        }

        painter->save();

        // QGraphicsScene applies the item's own opacity() to the painter
        // BEFORE calling paint() (it composites the whole paint() output
        // at that opacity) - without resetting it here, lowering an
        // image's opacity via Change Opacity also fades out this outline
        // and its handles, since they're drawn with the same painter at
        // the tail end of PixmapItem::paint()/TextItem::paint(). The
        // "peek" window-focus fade above is unaffected: it's baked into
        // selectColor's alpha channel, not painter opacity.
        painter->setOpacity(1.0);

        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        QColor selectColor = colorPreset[EPresetsColorIdx::kSelectionColor];
        selectColor.setAlphaF(selectColor.alphaF() * outlineOpacity);

        QPen pen(selectColor);
        pen.setWidth(selectLineWidth_);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(QBrush());

        // Draw the main selection rectangle
        painter->drawRect(this->bounding_rect_unselected());

        // If it's a single selection, draw the handles:
        if (static_cast<Mixin*>(this)->has_selection_handles() == true) {
            // Edit mode (TextItem, mid-typing): square caps - the visual
            // cue (matching PureRef) that a corner only resizes now, it
            // won't rotate (see hoverMoveEvent()/mousePressEvent() below).
            const bool resizeOnly
                = static_cast<Mixin*>(this)->paints_edit_mode_handles();
            pen.setWidth(selectHandleSize_);
            pen.setCapStyle(resizeOnly ? Qt::SquareCap : Qt::RoundCap);
            painter->setPen(pen);
            for (const QPointF& corner : corners()) {
                painter->drawPoint(corner);
            }

            // Smaller edge-midpoint handles - dragging these scales the
            // same way as the corners (see mousePressEvent()), just
            // anchored on the opposite edge instead of the opposite
            // corner. Same cap style as the corners.
            pen.setWidth(selectEdgeHandleSize_);
            pen.setCapStyle(resizeOnly ? Qt::SquareCap : Qt::RoundCap);
            painter->setPen(pen);
            for (const auto& edge : get_edge_bounds()) {
                painter->drawPoint(edge.rect.center());
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

    qreal scale_orig_factor() const override { return scaleOrigFactor_; }
    void set_scale_orig_factor(qreal value) override
    {
        scaleOrigFactor_ = value;
    }
    qreal rotate_orig_degrees() const override { return rotateOrigDegrees_; }
    void set_rotate_orig_degrees(qreal value) override
    {
        rotateOrigDegrees_ = value;
    }

    QPainterPath get_scale_bounds(const QPointF& corner, qreal margin = 0) const
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


    // The interactable shape of the edge (non-corner) scale handles.
    // These stretch around the edge of the item filling the areas
    // between the corner scale handles, e.g. for the bottom right corner:

    //    │E│
    //  ──┼─┼─┐
    //  EE│S│R│
    //  ──┼─┘ │
    //    │R R│
    //    └───┘

    std::vector<EdgeBounds> get_edge_bounds()
    {
        qreal outer_margin = select_resize_size() / 2;
        qreal inner_margin = select_resize_size() / 2;
        QPointF origin = this->bounding_rect_unselected().topLeft();

        std::vector<EdgeBounds> edgeBounds;
        edgeBounds.reserve(4);

        // Top
        edgeBounds.push_back({QRectF(origin.x() + inner_margin,
                                     origin.y() - outer_margin,
                                     this->width() - 2 * inner_margin,
                                     outer_margin + inner_margin),
                              true});

        // Bottom
        edgeBounds.push_back({QRectF(origin.x() + inner_margin,
                                     origin.y() + this->height() - inner_margin,
                                     this->width() - 2 * inner_margin,
                                     outer_margin + inner_margin),
                              true});

        // Left
        edgeBounds.push_back({QRectF(origin.x() - outer_margin,
                                     origin.y() + inner_margin,
                                     outer_margin + inner_margin,
                                     this->height() - 2 * inner_margin),
                              false});

        // Right
        edgeBounds.push_back({QRectF(origin.x() + this->width() - inner_margin,
                                     origin.y() + inner_margin,
                                     outer_margin + inner_margin,
                                     this->height() - 2 * inner_margin),
                              false});

        return edgeBounds;
    }

    QRectF boundingRect() const override
    {
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
        if (static_cast<const Mixin*>(this)->has_selection_handles() == true) {
            auto margin = select_resize_size() / 2;
            auto rect = this->bounding_rect_unselected().marginsAdded(
                QMarginsF(margin, margin, margin, margin));
            path.addRect(rect);

            // Edit mode: no rotate handles, so don't claim their hit
            // area either - a click just past the corner should reach
            // whatever's underneath (or the text cursor), not this item.
            if (!static_cast<const Mixin*>(this)->paints_edit_mode_handles()) {
                for (const QPointF& corner : corners()) {
                    path.addPath(this->get_rotate_bounds(corner));
                }
            }
        } else {
            path.addRect(this->bounding_rect_unselected());
        }

        return path;
    }

protected:
    // Qt's QGraphicsItem::setCursor()/unsetCursor() don't reliably update
    // the viewport cursor (QTBUG-4190); route through the scene's
    // cursor_changed/cursor_cleared signals instead, same as Python.
    void set_cursor(const QCursor& cursor)
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (scene) {
            emit scene->cursor_changed(cursor);
        }
    }

    void unset_cursor()
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (scene) {
            emit scene->cursor_cleared();
        }
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
        if (static_cast<Mixin*>(this)->has_selection_handles() == false) {
            return;
        }

        QPointF pos = event->pos();
        if (isInHandleFreeCenter(pos)) {
            this->unset_cursor();
            return;
        }

        // Edit mode: corners resize only, never rotate (see
        // paint_selectable() above).
        const bool resizeOnly
            = static_cast<Mixin*>(this)->paints_edit_mode_handles();
        for (const QPointF& corner : corners()) {
            if (isInScaleHandle(corner, pos)) {
                this->set_cursor(get_corner_scale_cursor(corner));
                return;
            } else if (!resizeOnly && isInRotateHandle(corner, pos)) {
                // TODOLATER: custom rotate icon
                this->set_cursor(QCursor(Qt::SizeAllCursor));
                return;
            }
        }

        for (const auto& edge : get_edge_bounds()) {
            if (isInEdgeHandle(edge.rect, pos)) {
                if (is_edge_vertical(edge)) {
                    this->set_cursor(QCursor(Qt::SizeVerCursor));
                } else {
                    this->set_cursor(QCursor(Qt::SizeHorCursor));
                }
                return;
            }
        }

        this->unset_cursor();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
        if (static_cast<Mixin*>(this)->has_selection_handles() == false) {
            this->unset_cursor();
        }
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
    {
        this->unset_cursor();
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
            //
            // T::, not QGraphicsItem:: - for TextItem, T is
            // QGraphicsTextItem, whose own mousePressEvent places the
            // text cursor / starts a drag-selection. Qualifying to
            // QGraphicsItem here would silently skip that (this is the
            // same bug as the two fallbacks below).
            T::mousePressEvent(event);
            return;
        }

        if (isInHandleFreeCenter(event->pos())) {
            //This area should always trigger regular move operations,
            //even if it is covered by selection scale/flip/... handles.
            //This ensures that small items can always still be moved/edited.
            T::mousePressEvent(event); // see comment above
            return;
        }

        // Edit mode: corners resize only, never rotate (see
        // paint_selectable() above).
        const bool resizeOnly
            = static_cast<Mixin*>(this)->paints_edit_mode_handles();

        if (event->button() == Qt::LeftButton
            && static_cast<Mixin*>(this)->has_selection_handles() == true) {
            for (auto& corner : corners()) {
                //Check if we are in one of the corner's scale areas
                if (get_scale_bounds(corner).contains(event->pos())) {
                    if (resizeOnly) {
                        // Edit mode: resize the field (both dimensions -
                        // it's a corner), not a uniform scale.
                        active_mode_ = kFieldResizeMode;
                        eventAnchor_ = this->mapToScene(
                            get_scale_anchor(corner));
                        const QPointF c = this->center();
                        fieldResizeAnchorRight_ = corner.x() < c.x();
                        fieldResizeAnchorBottom_ = corner.y() < c.y();
                        fieldResizeAffectsWidth_ = true;
                        fieldResizeAffectsHeight_ = true;
                        fieldResizeOrigSize_
                            = this->bounding_rect_unselected().size();
                        event->accept();
                        return;
                    }
                    active_mode_ = kScaleMode;
                    eventDirection_ = get_direction_from_center(
                        event->scenePos());
                    eventAnchor_ = this->mapToScene(get_scale_anchor(corner));
                    // Anchor is the OPPOSITE corner, so the relevant
                    // distance is corner-to-corner: the full diagonal.
                    scaleRefLength_ = qSqrt(this->width() * this->width()
                                            + this->height() * this->height());
                    for (auto& item :
                         static_cast<Mixin*>(this)->selection_action_items()) {
                        auto* baseItem = dynamic_cast<IBaseItem*>(item);
                        baseItem->set_scale_orig_factor(item->scale());
                    }
                    event->accept();
                    return;
                }

                //Check if we are in one of the corner's rotate areas
                if (!resizeOnly && isInRotateHandle(corner, event->pos())) {
                    active_mode_ = kRotateMode;
                    eventAnchor_ = this->center_scene_coords();
                    rotateStartAngle_ = get_rotate_angle(event->scenePos());
                    for (auto& item :
                         static_cast<Mixin*>(this)->selection_action_items()) {
                        auto* baseItem = dynamic_cast<IBaseItem*>(item);
                        baseItem->set_rotate_orig_degrees(item->rotation());
                    }
                    event->accept();
                    return;
                }
            }

            //Check if we are in one of the edge's scale areas - same
            //kScaleMode drag as a corner, just anchored on the opposite
            //edge (get_scale_anchor() mirrors any local point around the
            //item's center, corner or edge midpoint alike).
            for (const auto& edge : get_edge_bounds()) {
                if (isInEdgeHandle(edge.rect, event->pos())) {
                    if (resizeOnly) {
                        // Edit mode: resize the field along whichever
                        // one axis this edge governs, leaving the other
                        // dimension untouched.
                        active_mode_ = kFieldResizeMode;
                        eventAnchor_ = this->mapToScene(
                            get_scale_anchor(edge.rect.center()));
                        const QPointF c = this->center();
                        fieldResizeAnchorRight_ = !edge.vertical
                                                  && edge.rect.center().x()
                                                         < c.x();
                        fieldResizeAnchorBottom_ = edge.vertical
                                                   && edge.rect.center().y()
                                                          < c.y();
                        fieldResizeAffectsWidth_ = !edge.vertical;
                        fieldResizeAffectsHeight_ = edge.vertical;
                        fieldResizeOrigSize_
                            = this->bounding_rect_unselected().size();
                        event->accept();
                        return;
                    }
                    active_mode_ = kScaleMode;
                    eventDirection_ = get_direction_from_center(
                        event->scenePos());
                    eventAnchor_ = this->mapToScene(
                        get_scale_anchor(edge.rect.center()));
                    // Anchor is the midpoint of the OPPOSITE edge, so the
                    // relevant distance is edge-to-edge along that one
                    // axis - not the diagonal (that would be the
                    // anchor-to-CORNER distance, always longer than the
                    // anchor-to-this-edge-midpoint distance actually
                    // being dragged, which under-scaled the response and
                    // made the handle visibly lag behind the cursor).
                    scaleRefLength_ = edge.vertical ? this->height()
                                                    : this->width();
                    for (auto& item :
                         static_cast<Mixin*>(this)->selection_action_items()) {
                        auto* baseItem = dynamic_cast<IBaseItem*>(item);
                        baseItem->set_scale_orig_factor(item->scale());
                    }
                    event->accept();
                    return;
                }
            }
        }

        T::mousePressEvent(event); // see comment above
    }

    qreal get_scale_factor(QGraphicsSceneMouseEvent* event) const
    {
        // scaleRefLength_ (set in mousePressEvent() when the drag
        // started) is the anchor-to-handle distance at scale 1: the full
        // diagonal for a corner drag, or just that one edge's own
        // dimension for an edge-midpoint drag - using the diagonal for
        // both (as this used to) under-scaled edge drags, since the
        // diagonal is always longer than either side alone.
        QPointF p = event->scenePos() - eventStart_;
        QPointF direction = eventDirection_;
        qreal delta = QPointF::dotProduct(direction, p) / scaleRefLength_;
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

    // Whether dragging this edge visually affects the item's vertical
    // (height) extent, accounting for rotation - e.g. at 90°, the "top"
    // edge (vertical=true) now points sideways and behaves like a
    // left/right edge instead. Used to pick the resize cursor.
    bool is_edge_vertical(const EdgeBounds& edge) const
    {
        if (((this->rotation() > 45) && (this->rotation() < 135))
            || ((this->rotation() > 225) && (this->rotation() < 315))) {
            return !edge.vertical;
        } else {
            return edge.vertical;
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
                auto* baseItem = dynamic_cast<IBaseItem*>(item);
                baseItem->set_scale(baseItem->scale_orig_factor() * factor,
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
                auto* baseItem = dynamic_cast<IBaseItem*>(item);
                baseItem->set_rotation(baseItem->rotate_orig_degrees()
                                           + delta * baseItem->flip(),
                                       item->mapFromScene(eventAnchor_));
            }
            event->accept();
            return;
        } else if (active_mode_ == kFieldResizeMode) {
            // Local coordinates throughout (not the diagonal-projection
            // math get_scale_factor() uses) - a field resize just needs
            // "how far past the anchor is the mouse now", one axis at a
            // time. eventAnchor_ (scene, fixed since press) is remapped
            // to LOCAL coordinates fresh here, every call, rather than
            // cached: its local coordinate is exactly what's changing as
            // the field resizes (e.g. the right edge's local x is
            // whatever the CURRENT width is, not the width at press
            // time) - a cached value would only stay correct for the
            // near/origin-side handles, silently drifting for the far
            // side (which is exactly the bug this replaced).
            const QPointF localMouse = this->mapFromScene(event->scenePos());
            const QPointF localAnchor = this->mapFromScene(eventAnchor_);
            const QRectF cur = this->bounding_rect_unselected();
            const qreal newWidth = fieldResizeAffectsWidth_
                                       ? qAbs(localMouse.x() - localAnchor.x())
                                       : cur.width();
            const qreal newHeight = fieldResizeAffectsHeight_
                                        ? qAbs(localMouse.y() - localAnchor.y())
                                        : cur.height();
            static_cast<Mixin*>(this)->resize_field(newWidth,
                                                    newHeight,
                                                    fieldResizeAnchorRight_,
                                                    fieldResizeAnchorBottom_);
            event->accept();
            return;
        }

        T::mouseMoveEvent(event); // see mousePressEvent() comment above
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
            scene->on_selection_change(); // or emit selectionChange()
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
        } else if (active_mode_ == kFieldResizeMode) {
            static_cast<Mixin*>(this)
                ->commit_field_resize(fieldResizeOrigSize_.width(),
                                      fieldResizeOrigSize_.height(),
                                      fieldResizeAnchorRight_,
                                      fieldResizeAnchorBottom_);
            event->accept();
            resetActions();
            return;
        }

        resetActions();
        T::mouseReleaseEvent(event); // see mousePressEvent() comment above
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        paint_selectable(painter, option, widget);
    }

public slots:
    void on_view_scale_change() override { this->prepareGeometryChange(); }

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
    bool isInEdgeHandle(const QRectF& rect, const QPointF& pos) const
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
        FLOG_DEBUG(familiar::log::Ch::Items, "Initialized {}", toString());
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
    virtual bool has_selection_handles() const { return true; }
    virtual bool paints_edit_mode_handles() const { return false; }
    // See ItemMixin::resize_field()/commit_field_resize() (moveitem.h) -
    // never actually called here (paints_edit_mode_handles() is always
    // false), just needs to exist for the CRTP calls in selector.h to
    // compile.
    virtual void resize_field(qreal, qreal, bool, bool) {}
    virtual void commit_field_resize(qreal, qreal, bool, bool) {}

    // Only the multi-select bounding box (this item) fades with window
    // activation/hover - see CanvasView::updateSelectionVisibility() and
    // ItemMixin::fades_with_window_focus() (moveitem.h) for individual
    // items, which don't.
    virtual bool fades_with_window_focus() const { return true; }

    // Recursive - a flat scene()->selectedItems() alone misses a
    // GroupItem's own un-selected descendants (see IBaseItem::
    // nested_selection_action_items()'s own comment for why that
    // matters: without expanding into them here too, a group caught in
    // a multi-selection alongside other items gets its own members left
    // behind with only a crude position-cascade during a handle drag,
    // never actually scaled/rotated).
    virtual QVector<QGraphicsItem*> selection_action_items()
    {
        QVector<QGraphicsItem*> items;
        if (!this->scene())
            return items;
        QSet<QGraphicsItem*> seen;
        QList<QGraphicsItem*> queue = this->scene()->selectedItems();
        while (!queue.isEmpty()) {
            QGraphicsItem* item = queue.takeFirst();
            if (seen.contains(item))
                continue;
            seen.insert(item);
            items.append(item);
            if (auto* baseItem = dynamic_cast<IBaseItem*>(item))
                queue.append(baseItem->nested_selection_action_items());
        }
        return items;
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

        SelectableMixin<MultiSelectItem, QGraphicsRectItem>::mousePressEvent(
            event);
    }

public:
    IBaseItem* create_copy() override
    {
        Q_ASSERT_X(false,
                   "MultiSelectItem::create_copy",
                   "Should not be called");
        return nullptr;
    }
    // Matches Python's init_selectable() defaulting self.is_editable to
    // False for every SelectableMixin item (not an "unreachable" case:
    // double-clicking inside the multi-select area legitimately reaches
    // this, e.g. via CanvasScene::mouseDoubleClickEvent).
    bool is_editable() override { return false; }
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
    // Unlike is_image()/uid() etc., this genuinely gets called
    // during unfiltered scene iteration (itemAddByUser(), items_by_type())
    // since rubberband_item_/multiselect_item_ are real scene items while
    // active. Mirrors Python's getattr(i, 'TYPE', None): a safe "no type"
    // answer, not a logic error.
    std::string get_type() const override { return {}; }

    // No selection-driven bring-to-front logic for the multi-select
    // rectangle itself (Python's MultiSelectItem has no on_selected_change,
    // so BeeItemMixin's hasattr() guard silently skips it there).
    void on_selected_change(bool value) { Q_UNUSED(value) }
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
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Updated rubberband {}",
                   toString());
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
    // See MultiSelectItem::get_type(): rubberband_item_ is a real scene
    // item while dragging, so this is reached during unfiltered scene
    // iteration and must answer safely rather than assert.
    std::string get_type() const override { return {}; }
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
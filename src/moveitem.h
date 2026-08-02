#ifndef MOVEITEM_H
#define MOVEITEM_H

#include "commands.h"
#include "core/settingshandler.h"
#include "selector.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QMap>
#include <QObject>
#include <QPainter>
#include <QPair>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTextCursor>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>
#include <QWheelEvent>
#include <QtGlobal>
#include <qassert.h>

#include "log/log.h"

template<typename U, typename T>
class ItemMixin : public SelectableMixin<U, T>
{
public:
    explicit ItemMixin(T* parent = nullptr)
        : SelectableMixin<U, T>(parent)
    {}
    virtual void set_pos_center(const QPointF& pos)
    {
        this->setPos(pos - this->center_scene_coords());
    }

    virtual bool has_selection_outline() const { return this->isSelected(); }

    // See MultiSelectItem::fades_with_window_focus() (selector.h): an
    // individual item's own selection outline always stays fully
    // visible, regardless of window activation/hover - only the
    // multi-select bounding box participates in that fade.
    virtual bool fades_with_window_focus() const { return false; }

    virtual bool has_selection_handles() const
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        return this->isSelected() && scene && scene->has_single_selection();
    }

    // Draw square (not round) corner markers instead of the interactive
    // scale handles - TextItem returns true while in edit mode
    // (PureRef-style visual cue that clicks now edit text, not resize).
    virtual bool paints_edit_mode_handles() const { return false; }

    // Edit-mode (square-handle) drag target: resize the FIELD itself
    // (width/height) instead of the uniform set_scale() the round
    // handles use. anchorRight/anchorBottom say which side must stay
    // visually fixed (true = the right/bottom edge is the anchor,
    // i.e. the left/top handle is what's being dragged) - see
    // selector.h's kFieldResizeMode for how these are derived. Only
    // TextItem overrides this with a real implementation - everything
    // else never has paints_edit_mode_handles() true, so this is never
    // actually invoked for them; it only needs to exist so the CRTP call
    // in selector.h compiles for every Mixin.
    virtual void resize_field(qreal newWidth,
                              qreal newHeight,
                              bool anchorRight,
                              bool anchorBottom)
    {
        Q_UNUSED(newWidth);
        Q_UNUSED(newHeight);
        Q_UNUSED(anchorRight);
        Q_UNUSED(anchorBottom);
    }

    // Pushes the undo command for a completed kFieldResizeMode drag
    // (selector.h mouseReleaseEvent()). A separate hook from
    // resize_field() itself (which only applies the live value, called
    // on every mouseMove) because the actual QUndoCommand
    // (ResizeTextFieldCommand, commands.h) takes a concrete TextItem* -
    // selector.h can't construct one directly (it's included FROM
    // moveitem.h, so TextItem is only ever an incomplete forward
    // declaration from selector.h's side); only TextItem's own override
    // does anything, same reasoning as resize_field() above.
    virtual void commit_field_resize(qreal oldWidth,
                                     qreal oldHeight,
                                     bool anchorRight,
                                     bool anchorBottom)
    {
        Q_UNUSED(oldWidth);
        Q_UNUSED(oldHeight);
        Q_UNUSED(anchorRight);
        Q_UNUSED(anchorBottom);
    }

    virtual QList<QGraphicsItem*> selection_action_items()
    {
        return QList<QGraphicsItem*>() << this;
    }


    virtual void on_selected_change(bool value)
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        Q_ASSERT(scene);
        // Only bring to front when the user directly clicked this item
        // (kMoveMode). During a rubber-band drag (kRubberbandMode),
        // stacked/overlapping images would otherwise get reshuffled in
        // z-order just by being swept over, which is surprising and
        // unwanted - selecting shouldn't itself change stacking order.
        if (value && scene && !scene->has_selection()
            && scene->active_mode() == CanvasScene::ESceneMode::kMoveMode) {
            this->bring_to_front();
        }

        // fixed:
        // Ctrl+click excluding this item from an existing multi-selection:
        // send it behind whatever is still selected, so the remaining
        // selected images stay visually on top of the one just excluded
        // instead of it covering them.
        if (!value && scene
            && scene->active_mode() == CanvasScene::ESceneMode::kMoveMode) {
            QList<QGraphicsItem*> others;
            for (QGraphicsItem* other : scene->selectedItems(true)) {
                if (other != this) {
                    others.append(other);
                }
            }
            if (!others.isEmpty()) {
                qreal minZ = others.first()->zValue();
                for (QGraphicsItem* other : others) {
                    minZ = qMin(minZ, other->zValue());
                }
                this->set_z_value(minZ - scene->Z_STEP);
            }
        }
    }

    void update_from_data()
    {
        // TODOLATER:
    }
};

class PixmapItem : public ItemMixin<PixmapItem, QGraphicsPixmapItem>
{
public:
    const std::string TYPE = "pixmap"; // static constexpr
    const qreal CROP_HANDLE_SIZE = 15; // static constexpr
    using ColorGamut = QMap<QPair<int, int>, int>;
    using CropHandleFn = QRectF (PixmapItem::*)() const;
    QString filename_;
    bool is_image_{true};
    bool crop_mode = false;
    bool grayscale_ = false;
    QPixmap grayscalePixmap_{};
    mutable std::optional<ColorGamut> colorGamut_{};

    bool is_editable_ = false;
    QRectF crop_{};
    std::optional<QRectF> crop_temp{};
    std::optional<QPointF> crop_mode_event_start{};
    std::optional<CropHandleFn> crop_mode_move{};

    PixmapItem(const QImage& image,
               const QString& filename = QString(),
               QGraphicsPixmapItem* parent = nullptr)
        : ItemMixin<PixmapItem, QGraphicsPixmapItem>(parent)
        , filename_(filename)
    {
        setPixmap(QPixmap::fromImage(image));
        reset_crop();
        FLOG_DEBUG(familiar::log::Ch::Items, "Initialized {}", toString());
        crop_mode = false;
        init_selectable();
    }

    bool is_image() const override { return is_image_; }

    QString toString() const
    {
        QSize size = pixmap().size();
        return QString("Image \"%1\" %2 x %3")
            .arg(filename_)
            .arg(size.width())
            .arg(size.height());
    }

    // TODOLATER:
    // static PixmapItem* create_from_data() {}

    QRectF crop() { return crop_; }
    void set_crop(const QRectF& crop)
    {
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Setting crop for {} to {}",
                   toString(),
                   crop);
        this->prepareGeometryChange();
        this->crop_ = crop;
        this->update();
    }

    bool grayscale() const { return grayscale_; }
    void setGrayscale(bool value)
    {
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Setting grayscale for {} to {}",
                   toString(),
                   value);
        grayscale_ = value;
        if (value) {
            QImage img(pixmap().size(), QImage::Format_Grayscale8);
            auto colorPreset
                = SettingsHandler::getInstance()->getCurrentColorPreset();
            img.fill(colorPreset[EPresetsColorIdx::kCanvasColor]);
            QPainter painter(&img);
            painter.drawPixmap(0, 0, pixmap());
            painter.end();
            grayscalePixmap_ = QPixmap::fromImage(img);
        } else {
            grayscalePixmap_ = QPixmap();
        }
        update();
    }
    // TODOTALER: use standart type func
    std::string get_type() const override { return TYPE; }

    QColor sample_color_at(const QPointF& pos)
    {
        QPointF ipos = this->mapFromScene(pos);
        QPixmap pm = grayscale_ ? grayscalePixmap_ : pixmap();
        QImage img = pm.toImage();

        QColor color = img.pixelColor((int) ipos.x(), (int) ipos.y());
        if (color.alpha()) {
            return color;
        }
        return QColor();
    }

    QRectF bounding_rect_unselected() const override
    {
        if (crop_mode) {
            //ItemMixin<PixmapItem, QGraphicsPixmapItem>::bounding_rect_unselected();
            return QGraphicsPixmapItem::boundingRect();
        }

        return crop_;
    }

    QVariantMap get_extra_save_data() const override
    {
        QVariantMap data;
        data[QStringLiteral("filename")] = filename_;
        data[QStringLiteral("opacity")] = opacity();
        data[QStringLiteral("grayscale")] = grayscale_;
        data[QStringLiteral("crop")] = QVariantList{crop_.topLeft().x(),
                                                    crop_.topLeft().y(),
                                                    crop_.width(),
                                                    crop_.height()};
        return data;
    }

    // TODOLATER: not wired up to a caller yet (batch export).
    QString get_filename_for_export(const QString& imgformat) const
    {
        QString id = uid().toString(QUuid::WithoutBraces);

        if (!filename_.isEmpty()) {
            QString basename = QFileInfo(filename_).completeBaseName();
            return QString("%1-%2.%3").arg(id, basename, imgformat);
        }
        return QString("%1.%2").arg(id, imgformat);
    }

    // Determines the format for storing this image.
    QString get_imgformat(const QImage& img) const
    {
        QString formt = SettingsHandler::getInstance()->imageStorageFormat();

        if (formt == QLatin1String("best")) {
            if (img.hasAlphaChannel()
                || (img.height() < 500 && img.width() < 500)) {
                formt = QStringLiteral("png");
            } else {
                formt = QStringLiteral("jpg");
            }
        }

        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Found format {} for {}",
                   formt,
                   toString());
        return formt;
    }

    std::pair<QByteArray, QString> pixmap_to_bytes(bool apply_grayscale = false,
                                                   bool apply_crop = false)
    {
        QByteArray barray;
        QBuffer buffer(&barray);
        buffer.open(QIODevice::WriteOnly);

        QPixmap pm = (apply_grayscale && grayscale_) ? grayscalePixmap_
                                                     : pixmap();
        if (apply_crop) {
            pm = pm.copy(crop_.toRect());
        }

        QImage img = pm.toImage();
        QString imgformat = get_imgformat(img);
        img.save(&buffer, imgformat.toUpper().toUtf8().constData(), 90);
        return {barray, imgformat};
    }

    void setPixmap(const QPixmap& pixmap)
    {
        QGraphicsPixmapItem::setPixmap(pixmap);
        this->reset_crop();
    }

    void pixmap_from_bytes(const QByteArray& bytes)
    {
        QPixmap pixmap;
        pixmap.loadFromData(bytes);
        this->setPixmap(pixmap);
    }

    // set_image function
    bool is_editable() override { return is_editable_; }


    IBaseItem* create_copy() override
    {
        auto* item = new PixmapItem(QImage(), filename_);
        item->setPixmap(pixmap());
        item->setPos(pos());
        item->setZValue(zValue());
        item->setScale(scale());
        item->setRotation(rotation());
        item->setOpacity(opacity());
        item->setGrayscale(grayscale_);
        if (flip() == -1) {
            item->do_flip();
        }
        item->set_crop(crop_);
        return item;
    }

    const ColorGamut& color_gamut() const
    {
        if (!colorGamut_) {
            FLOG_DEBUG(familiar::log::Ch::Items,
                       "Calculating color gamut for {}",
                       toString());
            ColorGamut gamut;
            QImage img = pixmap().toImage();
            // Don't evaluate every pixel for larger images:
            int step = std::max(1,
                                static_cast<int>(
                                    std::max(img.width(), img.height()) / 1000));
            FLOG_DEBUG(familiar::log::Ch::Items,
                       "Considering every {}. row/column",
                       step);

            for (int i = 0; i < img.width(); i += step) {
                for (int j = 0; j < img.height(); j += step) {
                    QColor rgb = img.pixelColor(i, j);
                    int r = rgb.red(), g = rgb.blue(), b = rgb.green();
                    if (rgb.alpha() > 5 && std::min({r, g, b}) < 250
                        && std::max({r, g, b}) > 5) {
                        // Only consider pixels that aren't close to
                        // transparent, white or black
                        gamut[qMakePair(rgb.hue(), rgb.saturation())]++;
                    }
                }
            }

            FLOG_DEBUG(familiar::log::Ch::Items,
                       "Got {} color gamut values",
                       gamut.size());
            colorGamut_ = gamut;
        }
        return *colorGamut_;
    }

    void copy_to_clipboard(QClipboard* clipboard)
    {
        clipboard->setPixmap(this->pixmap());
    }

    void reset_crop()
    {
        crop_ = QRectF(0,
                       0,
                       this->pixmap().size().width(),
                       this->pixmap().size().height());
    }

    qreal crop_handle_size() const
    {
        return this->fixed_length_for_viewport(CROP_HANDLE_SIZE);
    }

    QRectF crop_handle_topleft() const
    {
        QPointF topLeft = crop_temp->topLeft();
        return QRectF(topLeft.x(),
                      topLeft.y(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_handle_bottomleft() const
    {
        QPointF bottomLeft = crop_temp->bottomLeft();
        return QRectF(bottomLeft.x(),
                      bottomLeft.y() - crop_handle_size(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_handle_bottomright() const
    {
        QPointF bottomRight = crop_temp->bottomRight();
        return QRectF(bottomRight.x() - crop_handle_size(),
                      bottomRight.y() - crop_handle_size(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_handle_topright() const
    {
        QPointF topRight = crop_temp->topRight();
        return QRectF(topRight.x() - crop_handle_size(),
                      topRight.y(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    QList<CropHandleFn> crop_handles() const
    {
        return {&PixmapItem::crop_handle_topleft,
                &PixmapItem::crop_handle_bottomleft,
                &PixmapItem::crop_handle_bottomright,
                &PixmapItem::crop_handle_topright};
    }

    QRectF crop_edge_top() const
    {
        QPointF topLeft = crop_temp->topLeft();
        return QRectF(topLeft.x() + crop_handle_size(),
                      topLeft.y(),
                      crop_temp->width() - 2 * crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_edge_left() const
    {
        QPointF topLeft = crop_temp->topLeft();
        return QRectF(topLeft.x(),
                      topLeft.y() + crop_handle_size(),
                      crop_handle_size(),
                      crop_temp->height() - 2 * crop_handle_size());
    }

    QRectF crop_edge_bottom() const
    {
        QPointF bottomLeft = crop_temp->bottomLeft();
        return QRectF(bottomLeft.x() + crop_handle_size(),
                      bottomLeft.y() - crop_handle_size(),
                      crop_temp->width() - 2 * crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_edge_right() const
    {
        QPointF topRight = crop_temp->topRight();
        return QRectF(topRight.x() - crop_handle_size(),
                      topRight.y() + crop_handle_size(),
                      crop_handle_size(),
                      crop_temp->height() - 2 * crop_handle_size());
    }

    // Function to return all crop edge functions as a tuple
    QList<CropHandleFn> crop_edges() const
    {
        return {&PixmapItem::crop_edge_top,
                &PixmapItem::crop_edge_left,
                &PixmapItem::crop_edge_bottom,
                &PixmapItem::crop_edge_right};
    }

    Qt::CursorShape get_crop_handle_cursor(CropHandleFn handle)
    {
        bool is_topleft_or_bottomright
            = (handle == &PixmapItem::crop_handle_topleft
               || handle == &PixmapItem::crop_handle_bottomright);
        return get_diag_cursor(is_topleft_or_bottomright);
    }

    Qt::CursorShape get_crop_edge_cursor(CropHandleFn edge)
    {
        bool top_or_bottom = (edge == &PixmapItem::crop_edge_top
                              || edge == &PixmapItem::crop_edge_bottom);

        bool sideways = (45 < rotation() && rotation() < 135)
                        || (225 < rotation() && rotation() < 315);

        return (top_or_bottom == sideways) ? Qt::SizeHorCursor
                                           : Qt::SizeVerCursor;
    }

    // Returns the point, or the nearest point within the pixmap.
    QPointF ensure_point_within_crop_bounds(const QPointF& point,
                                            CropHandleFn handle) const
    {
        QPointF topleft;
        QPointF bottomright;
        QSize pixmapSize = pixmap().size();

        if (handle == &PixmapItem::crop_handle_topleft) {
            topleft = QPointF(0, 0);
            bottomright = crop_temp->bottomRight();
        } else if (handle == &PixmapItem::crop_handle_bottomleft) {
            topleft = QPointF(0, crop_temp->top());
            bottomright = QPointF(crop_temp->right(), pixmapSize.height());
        } else if (handle == &PixmapItem::crop_handle_bottomright) {
            topleft = crop_temp->topLeft();
            bottomright = QPointF(pixmapSize.width(), pixmapSize.height());
        } else if (handle == &PixmapItem::crop_handle_topright) {
            topleft = QPointF(crop_temp->left(), 0);
            bottomright = QPointF(pixmapSize.width(), crop_temp->bottom());
        } else if (handle == &PixmapItem::crop_edge_top) {
            topleft = QPointF(0, 0);
            bottomright = QPointF(pixmapSize.width(), crop_temp->bottom());
        } else if (handle == &PixmapItem::crop_edge_bottom) {
            topleft = QPointF(0, crop_temp->top());
            bottomright = QPointF(pixmapSize.width(), pixmapSize.height());
        } else if (handle == &PixmapItem::crop_edge_left) {
            topleft = QPointF(0, 0);
            bottomright = QPointF(crop_temp->right(), pixmapSize.height());
        } else if (handle == &PixmapItem::crop_edge_right) {
            topleft = QPointF(crop_temp->left(), 0);
            bottomright = QPointF(pixmapSize.width(), pixmapSize.height());
        }

        QPointF result = point;
        result.setX(
            std::min(bottomright.x(), std::max(topleft.x(), result.x())));
        result.setY(
            std::min(bottomright.y(), std::max(topleft.y(), result.y())));
        return result;
    }

    void draw_crop_rect(QPainter& painter, const QRectF& rect)
    {
        QPen pen(Qt::white);
        pen.setWidth(2);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.drawRect(rect);
        pen.setColor(Qt::black);
        pen.setStyle(Qt::DotLine);
        painter.setPen(pen);
        painter.drawRect(rect);
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override
    {
        if (std::abs(painter->combinedTransform().m11()) < 2) {
            painter->setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);
        }
        if (crop_mode) {
            // TODOLATER:
            // paint_debug(painter, option, widget);

            // Darken image outside of cropped area
            painter->drawPixmap(0, 0, pixmap());
            QPainterPath path;
            path.addRect(crop_temp.value());
            QColor color(0, 0, 0);
            color.setAlpha(100);
            painter->setBrush(QBrush(color));
            painter->setPen(Qt::NoPen);
            painter->drawPath(path);
            painter->setBrush(Qt::NoBrush);

            // Draw crop handles
            for (auto handle : crop_handles()) {
                draw_crop_rect(*painter, (this->*handle)());
            }

            draw_crop_rect(*painter, *crop_temp);
        } else {
            const QPixmap& pm = grayscale_ ? grayscalePixmap_ : pixmap();
            painter->drawPixmap(crop_, pm, crop_);
            paint_selectable(painter, option, widget);
        }
    }

    void enter_crop_mode() override
    {
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Entering crop mode on {}",
                   toString());
        this->prepareGeometryChange();
        crop_mode = true;
        crop_temp = crop();
        crop_mode_move = std::nullopt;
        crop_mode_event_start = std::nullopt;
        this->grabKeyboard();
        this->update();
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->crop_item = this;
    }

    void exit_crop_mode(bool confirm)
    {
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Exiting crop mode with {} on {}",
                   confirm,
                   toString());
        if (confirm && crop() != *crop_temp) {
            auto* scene = dynamic_cast<CanvasScene*>(this->scene());
            // TODOLATER: interface
            scene->undo_stack_->push(
                new CropItemCommand(this, crop_temp.value()));
        }

        this->prepareGeometryChange();
        crop_mode = false;
        crop_temp = std::nullopt;
        crop_mode_move = std::nullopt;
        crop_mode_event_start = std::nullopt;
        this->ungrabKeyboard();
        this->update();
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->crop_item = nullptr;
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            exit_crop_mode(true);
        } else if (event->key() == Qt::Key_Escape) {
            exit_crop_mode(false);
        } else {
            // Call the base class implementation for other keys
            QGraphicsItem::keyPressEvent(event);
        }
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
        if (!crop_mode) {
            ItemMixin<PixmapItem, QGraphicsPixmapItem>::hoverMoveEvent(event);
            return;
        }

        for (auto handle : crop_handles()) {
            if ((this->*handle)().contains(event->pos())) {
                set_cursor(get_crop_handle_cursor(handle));
                return;
            }
        }

        for (auto edge : crop_edges()) {
            if ((this->*edge)().contains(event->pos())) {
                set_cursor(get_crop_edge_cursor(edge));
                return;
            }
        }

        unset_cursor();
        // setCursor(Qt::ArrowCursor);
        // unset_cursor();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (!crop_mode) {
            ItemMixin<PixmapItem, QGraphicsPixmapItem>::mousePressEvent(event);
            return;
        }

        event->accept();

        for (auto handle : crop_handles()) {
            // Click into a handle?
            if ((this->*handle)().contains(event->pos())) {
                crop_mode_event_start = event->pos();
                crop_mode_move = handle;
                return;
            }
        }

        for (auto edge : crop_edges()) {
            // Click into an edge handle?
            if ((this->*edge)().contains(event->pos())) {
                crop_mode_event_start = event->pos();
                crop_mode_move = edge;
                return;
            }
        }

        // Click not in handle, end cropping mode:
        exit_crop_mode(crop_temp->contains(event->pos()));
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (crop_mode && crop_mode_move && crop_mode_event_start) {
            QPointF diff = event->pos() - *crop_mode_event_start;
            CropHandleFn move = *crop_mode_move;

            if (move == &PixmapItem::crop_handle_topleft) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->topLeft()
                                                          + diff,
                                                      move);
                crop_temp->setTopLeft(newPoint);
            } else if (move == &PixmapItem::crop_handle_bottomleft) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->bottomLeft()
                                                          + diff,
                                                      move);
                crop_temp->setBottomLeft(newPoint);
            } else if (move == &PixmapItem::crop_handle_bottomright) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->bottomRight()
                                                          + diff,
                                                      move);
                crop_temp->setBottomRight(newPoint);
            } else if (move == &PixmapItem::crop_handle_topright) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->topRight()
                                                          + diff,
                                                      move);
                crop_temp->setTopRight(newPoint);
            } else if (move == &PixmapItem::crop_edge_top) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->topLeft()
                                                          + diff,
                                                      move);
                crop_temp->setTop(newPoint.y());
            } else if (move == &PixmapItem::crop_edge_left) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->topLeft()
                                                          + diff,
                                                      move);
                crop_temp->setLeft(newPoint.x());
            } else if (move == &PixmapItem::crop_edge_bottom) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->bottomLeft()
                                                          + diff,
                                                      move);
                crop_temp->setBottom(newPoint.y());
            } else if (move == &PixmapItem::crop_edge_right) {
                QPointF newPoint
                    = ensure_point_within_crop_bounds(crop_temp->topRight()
                                                          + diff,
                                                      move);
                crop_temp->setRight(newPoint.x());
            }

            update();
            crop_mode_event_start = event->pos();
            event->accept();
        } else {
            ItemMixin<PixmapItem, QGraphicsPixmapItem>::mouseMoveEvent(event);
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (crop_mode) {
            crop_mode_move = std::nullopt;
            crop_mode_event_start = std::nullopt;
            event->accept();
        } else {
            ItemMixin<PixmapItem, QGraphicsPixmapItem>::mouseReleaseEvent(event);
        }
    }
};

class TextItem : public ItemMixin<TextItem, QGraphicsTextItem>
{
public:
    const std::string TYPE = "text"; // static constexpr
    bool edit_mode = false;
    QString old_html;

    // Default note fill - the backdrop TextItem always painted, now
    // per-item and persisted (PureRef-style notes, roadmap step 9).
    static QColor default_fill_color() { return QColor(0, 0, 0, 40); }

    TextItem(const QString& text = QString(),
             QGraphicsTextItem* parent = nullptr)
        : ItemMixin<TextItem, QGraphicsTextItem>(parent)
        , fill_color_(default_fill_color())
    {
        setPlainText(text.isEmpty() ? QStringLiteral("Text") : text);

        init_selectable();
        edit_mode = false;
        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        setDefaultTextColor(colorPreset[EPresetsColorIdx::kTextColor]);
        FLOG_DEBUG(familiar::log::Ch::Items, "Initialized {}", toString());
    }

    bool is_image() const override { return false; }
    std::string get_type() const override { return TYPE; }
    // int type() const override { return 666; }
    bool is_editable() override { return true; }

    QString toString() const
    {
        return QString("Text \"%1\"").arg(this->toPlainText().left(40));
    }

    static TextItem* create_from_data(const QVariantMap& data = QVariantMap())
    {
        auto* item = new TextItem(data.value(QStringLiteral("text")).toString());
        item->apply_extra_save_data(data);
        return item;
    }

    QVariantMap get_extra_save_data() const override
    {
        QVariantMap data;
        // Plain text stays alongside the html: forward-compatible
        // fallback and greppable manifests (docs/fml_format_design.md).
        data[QStringLiteral("text")] = this->toPlainText();
        data[QStringLiteral("html")] = this->toHtml();
        if (fill_color_ != default_fill_color())
            data[QStringLiteral("fill_color")]
                = fill_color_.name(QColor::HexArgb);
        // Manual field size from the edit-mode square handles
        // (resize_field()) - absent entirely for the common case (never
        // manually resized), same -1-is-auto convention as
        // document()->textWidth() itself.
        if (this->document()->textWidth() >= 0)
            data[QStringLiteral("field_width")] = this->document()->textWidth();
        if (manualHeight_ >= 0)
            data[QStringLiteral("field_height")] = manualHeight_;
        return data;
    }

    // Counterpart of get_extra_save_data() minus "text" (the caller
    // decides how the initial plain text reaches the constructor).
    void apply_extra_save_data(const QVariantMap& data)
    {
        const QString html = data.value(QStringLiteral("html")).toString();
        if (!html.isEmpty())
            this->setHtml(html);
        const QString fill = data.value(QStringLiteral("fill_color")).toString();
        if (!fill.isEmpty()) {
            QColor c(fill);
            if (c.isValid())
                fill_color_ = c;
        }
        if (data.contains(QStringLiteral("field_width")))
            this->document()->setTextWidth(
                data.value(QStringLiteral("field_width")).toReal());
        if (data.contains(QStringLiteral("field_height")))
            manualHeight_ = data.value(QStringLiteral("field_height")).toReal();
    }

    QColor fill_color() const { return fill_color_; }
    void set_fill_color(const QColor& color)
    {
        // TEMPORARY debug logging (roadmap step 9 fill-color
        // investigation) - remove once the BG/H no-op bug is confirmed
        // fixed.
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "set_fill_color: {},{},{},{} -> {},{},{},{}",
                   fill_color_.red(),
                   fill_color_.green(),
                   fill_color_.blue(),
                   fill_color_.alpha(),
                   color.red(),
                   color.green(),
                   color.blue(),
                   color.alpha());
        fill_color_ = color;
        update();
    }

    bool contains(const QPointF& point) const override
    {
        return this->boundingRect().contains(point);
    }

    // Natural content size unless the edit-mode square handles
    // (resize_field() below) stretched the field taller than the text
    // actually needs - width never needs the same treatment, since
    // setTextWidth() already directly drives QGraphicsTextItem's own
    // boundingRect(), wrapping included.
    QRectF bounding_rect_unselected() const override
    {
        QRectF rect = QGraphicsTextItem::boundingRect();
        if (manualHeight_ > rect.height())
            rect.setHeight(manualHeight_);
        return rect;
    }

    // Edit-mode (square handle) drag target - see moveitem.h's
    // ItemMixin::resize_field() and selector.h's kFieldResizeMode.
    // Unlike the round handles' uniform set_scale(), this changes the
    // field's own width (word-wrap via setTextWidth(), Qt handles the
    // rest) and height (manualHeight_, only ever pads empty space below
    // the text - see bounding_rect_unselected() above).
    //
    // anchorRight/anchorBottom say which side must stay visually fixed.
    // Unlike set_scale()/set_rotation(), setTextWidth()/manualHeight_
    // only change the DOCUMENT layout, never this item's own transform -
    // so the "capture mapToScene(anchor) before and after, shift pos() by
    // the difference" trick those use doesn't apply here at all: with no
    // transform change, that difference is always exactly zero
    // regardless of which point you pick. What actually has to happen
    // instead: work out in LOCAL terms how far the near (unanchored) side
    // moved by directly (deltaWidth/deltaHeight), then shift pos() by
    // that LOCAL vector re-expressed in scene space (mapToScene(v) -
    // mapToScene(origin) isolates just the rotation/scale part of the
    // transform, which is exactly the part a plain size change needs to
    // go through - the translation cancels out of the subtraction, so
    // this part of the trick is still valid).
    //
    // newWidth/newHeight < 0 means "auto" for that dimension (matching
    // QTextDocument::textWidth()'s own -1-is-auto convention) - used by
    // reset_manual_size() and by ResizeTextFieldCommand's undo() (the
    // "old" side of an undo can itself be "was auto" if the very first
    // manual resize is undone).
    void resize_field(qreal newWidth,
                      qreal newHeight,
                      bool anchorRight,
                      bool anchorBottom) override
    {
        constexpr qreal kMinFieldSize = 20.0;
        this->prepareGeometryChange();

        const qreal oldWidth = document()->textWidth() < 0
                                  ? QGraphicsTextItem::boundingRect().width()
                                  : document()->textWidth();
        const qreal oldHeight = manualHeight_ < 0
                                   ? QGraphicsTextItem::boundingRect().height()
                                   : manualHeight_;

        const qreal clampedWidth
            = newWidth < 0 ? -1 : qMax(newWidth, kMinFieldSize);
        const qreal clampedHeight
            = newHeight < 0 ? -1 : qMax(newHeight, kMinFieldSize);

        document()->setTextWidth(clampedWidth);
        manualHeight_ = clampedHeight;

        const qreal deltaW = (clampedWidth < 0 ? oldWidth : clampedWidth)
                            - oldWidth;
        const qreal deltaH = (clampedHeight < 0 ? oldHeight : clampedHeight)
                            - oldHeight;
        const QPointF localShift(anchorRight ? -deltaW : 0.0,
                                 anchorBottom ? -deltaH : 0.0);
        if (!localShift.isNull()) {
            const QPointF origin = this->mapToScene(QPointF(0, 0));
            const QPointF shifted = this->mapToScene(localShift);
            this->setPos(this->pos() + (shifted - origin));
        }
        update();
    }

    // Pushes the undo command for a completed drag (selector.h
    // mouseReleaseEvent(), kFieldResizeMode) - the live values were
    // already applied by resize_field() on every mouseMove, so this
    // just needs the "old" side and ignoreFirstRedo (see
    // ResizeTextFieldCommand, commands.h).
    void commit_field_resize(qreal oldWidth,
                             qreal oldHeight,
                             bool anchorRight,
                             bool anchorBottom) override
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (!scene)
            return;
        scene->undo_stack_->push(
            new ResizeTextFieldCommand(this,
                                       document()->textWidth(),
                                       manualHeight_,
                                       oldWidth,
                                       oldHeight,
                                       anchorRight,
                                       anchorBottom,
                                       /*ignoreFirstRedo=*/true));
    }

    // "Autosize" toolbar button (ui/text_edit_toolbar.cpp) - undoes any
    // manual field sizing from resize_field(), snapping back to however
    // big the text naturally needs to be. anchorRight=anchorBottom=false
    // (top-left/origin side is the anchor) since there's no dragged
    // handle here to determine a side from, and the origin never moves
    // due to size alone anyway.
    void reset_manual_size()
    {
        const qreal oldWidth = document()->textWidth();
        const qreal oldHeight = manualHeight_;
        if (oldWidth < 0 && oldHeight < 0)
            return; // already natural size
        resize_field(-1, -1, false, false);
        if (auto* scene = dynamic_cast<CanvasScene*>(this->scene())) {
            scene->undo_stack_->push(new ResizeTextFieldCommand(
                this, -1, -1, oldWidth, oldHeight, false, false, true));
        }
    }

    bool has_manual_size() const
    {
        return document()->textWidth() >= 0 || manualHeight_ >= 0;
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        // TEMPORARY debug logging (roadmap step 9 fill-color
        // investigation) - remove once the BG/H no-op bug is confirmed
        // fixed. Throttled to once per actual value (paint() runs every
        // frame) so this doesn't flood the log.
        if (fill_color_ != lastPaintLoggedFillColor_) {
            lastPaintLoggedFillColor_ = fill_color_;
            FLOG_DEBUG(familiar::log::Ch::Items,
                       "paint(): drawing fill_color_ = {},{},{},{}",
                       fill_color_.red(),
                       fill_color_.green(),
                       fill_color_.blue(),
                       fill_color_.alpha());
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(fill_color_));
        // bounding_rect_unselected(), not QGraphicsTextItem::boundingRect()
        // directly: the manual-height padding from resize_field() needs
        // to actually show up as extra colored space, not just affect
        // hit-testing/selection UI.
        painter->drawRect(this->bounding_rect_unselected());
        QStyleOptionGraphicsItem updatedOption(*option);
        updatedOption.state = QStyle::State_Enabled;
        QGraphicsTextItem::paint(painter, &updatedOption, widget);
        this->paint_selectable(painter, option, widget);
    }

    IBaseItem* create_copy() override
    {
        auto* new_item = new TextItem(this->toPlainText());
        new_item->setHtml(this->toHtml());
        new_item->set_fill_color(fill_color_);
        new_item->document()->setTextWidth(this->document()->textWidth());
        new_item->manualHeight_ = manualHeight_;
        new_item->setPos(this->pos());
        new_item->setZValue(this->zValue());
        new_item->setScale(this->scale());
        new_item->setRotation(this->rotation());
        if (this->flip() == -1) {
            new_item->do_flip();
        }

        return new_item;
    }

    void enter_edit_mode()
    {
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Entering edit mode on {}",
                   toString());
        edit_mode = true;
        // html, not plain text: the same commit-on-exit diff also carries
        // any formatting the floating toolbar applied during the session.
        old_html = this->toHtml();
        old_fill_color_ = fill_color_;
        this->setTextInteractionFlags(Qt::TextEditorInteraction);
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->edit_item = this;
        scene->notify_edit_item_changed(this);
    }

    void exit_edit_mode(bool commit = true)
    {
        FLOG_DEBUG(familiar::log::Ch::Items,
                   "Exiting edit mode on {}",
                   toString());
        edit_mode = false;
        // Reset selection:
        this->setTextCursor(QTextCursor(document()));
        this->setTextInteractionFlags(Qt::NoTextInteraction);
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->edit_item = nullptr;
        scene->notify_edit_item_changed(nullptr);
        if (commit) {
            scene->undo_stack_->push(new ChangeTextCommand(this,
                                                           this->toHtml(),
                                                           old_html,
                                                           fill_color_,
                                                           old_fill_color_));
            if (this->toPlainText().trimmed().isEmpty()) {
                FLOG_DEBUG(familiar::log::Ch::Items, "Removing empty text item");
                scene->undo_stack_->push(
                    new DeleteItemsCommand(scene, QList<QGraphicsItem*>{this}));
            }
        } else {
            setHtml(old_html);
            set_fill_color(old_fill_color_);
        }
    }

    bool has_selection_handles() const override
    {
        // Handles stay live in edit mode too - PureRef lets you resize a
        // note while typing. paints_edit_mode_handles() below switches
        // corners to resize-only/square (see selector.h paint_selectable/
        // hoverMoveEvent/mousePressEvent/shape()) instead of disabling
        // them outright, which used to make clicking near the item's
        // edges do nothing while editing.
        return ItemMixin<TextItem, QGraphicsTextItem>::has_selection_handles();
    }

    bool paints_edit_mode_handles() const override { return edit_mode; }

    void copy_to_clipboard(QClipboard* clipboard)
    {
        clipboard->setText(this->toPlainText());
    }
    void enter_crop_mode() override
    {
        Q_ASSERT_X(false, "TextItem::enter_crop_mode", "Should not be called");
    }


protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        // Ctrl+click a hyperlink (inserted via the floating toolbar's
        // link popup, ui/text_edit_toolbar.cpp) to open it - same
        // convention as most code/text editors, so a PLAIN click while
        // editing still just moves the text cursor there like normal
        // typing, instead of yanking focus away to a browser/file
        // manager every time you click near a link to keep editing.
        // Not gated on edit_mode: the document layout can be queried
        // regardless, so this works the same whether the note is
        // currently being edited or just selected.
        if (event->button() == Qt::LeftButton
            && event->modifiers() == Qt::ControlModifier) {
            const QString href
                = document()->documentLayout()->anchorAt(event->pos());
            if (!href.isEmpty()) {
                QDesktopServices::openUrl(QUrl(href));
                event->accept();
                return;
            }
        }
        ItemMixin<TextItem, QGraphicsTextItem>::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        // Enter/Return used to commit-and-exit here, back when notes
        // were effectively single-line. Now that editing supports real
        // multi-line content (lists, wrapped paragraphs), it has to
        // insert a newline like any other text editor instead - exiting
        // is Escape or clicking elsewhere on the canvas (both commit, see
        // CanvasScene::mousePressEvent()'s edit_item check).
        if (event->key() == Qt::Key_Escape
            && event->modifiers() == Qt::NoModifier) {
            exit_edit_mode(true);
            event->accept();
            return;
        }
        // QGraphicsTextItem's default handling inserts a literal tab
        // character - since that's plain text, not list/indent
        // formatting, it silently survives even after the list itself is
        // later removed via the toolbar. Swallow Tab/Shift+Tab instead of
        // letting anything be typed.
        if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
            event->accept();
            return;
        }
        QGraphicsTextItem::keyPressEvent(event);
    }

private:
    QColor fill_color_;
    QColor old_fill_color_;
    // TEMPORARY (see paint() above) - remove together with that logging.
    QColor lastPaintLoggedFillColor_;
    // -1 = natural (content-driven) height, matching document()->
    // textWidth()'s own -1-means-auto convention - see
    // bounding_rect_unselected()/resize_field()/reset_manual_size().
    qreal manualHeight_{-1};
};

// Displayed instead of an item that couldn't be loaded from a save file.
// Won't itself be saved; the original item's data is preserved unless this
// stand-in gets deleted or the file is saved again.
class ErrorItem : public ItemMixin<ErrorItem, QGraphicsTextItem>
{
public:
    const std::string TYPE = "error"; // static constexpr
    // The uid of the manifest item this stand-in couldn't load (see
    // docs/fml_format_design.md §5.1/§6) - preserved so a re-save doesn't
    // mint a new identity for data that's otherwise round-tripped as-is.
    // Null if unknown (e.g. the manifest item itself was malformed).
    QUuid original_uid{};

    ErrorItem(const QString& text = QString(),
              QGraphicsTextItem* parent = nullptr)
        : ItemMixin<ErrorItem, QGraphicsTextItem>(parent)
    {
        setPlainText(text.isEmpty() ? QStringLiteral("Text") : text);
        init_selectable();
        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        setDefaultTextColor(colorPreset[EPresetsColorIdx::kTextColor]);
        FLOG_DEBUG(familiar::log::Ch::Items, "Initialized {}", toString());
    }

    bool is_image() const override { return false; }
    std::string get_type() const override { return TYPE; }
    bool is_editable() override { return false; }

    QString toString() const
    {
        return QString("Error \"%1\"").arg(this->toPlainText().left(40));
    }

    static ErrorItem* create_from_data(const QVariantMap& data = QVariantMap())
    {
        return new ErrorItem(data.value(QStringLiteral("text")).toString());
    }

    bool contains(const QPointF& point) const override
    {
        return this->boundingRect().contains(point);
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(QColor(200, 0, 0)));
        painter->drawRect(QGraphicsTextItem::boundingRect());
        QStyleOptionGraphicsItem updatedOption(*option);
        updatedOption.state = QStyle::State_Enabled;
        QGraphicsTextItem::paint(painter, &updatedOption, widget);
        this->paint_selectable(painter, option, widget);
    }

    void update_from_data()
    {
        // TODOLATER: kwargs-driven data loading isn't wired up yet;
        // Python sets original_uid/pos/z/scale/rotation from the
        // loaded data here.
    }

    IBaseItem* create_copy() override
    {
        auto* new_item = new ErrorItem(this->toPlainText());
        new_item->setPos(this->pos());
        new_item->setZValue(this->zValue());
        new_item->setScale(this->scale());
        new_item->setRotation(this->rotation());
        return new_item;
    }

    // Never display error messages flipped.
    qreal flip() const override { return 1; }

    // Never flip error messages.
    void do_flip(bool vertical = false,
                 const QPointF& anchor = QPointF(0, 0)) override
    {
        Q_UNUSED(vertical);
        Q_UNUSED(anchor);
    }

    void enter_crop_mode() override
    {
        Q_ASSERT_X(false, "ErrorItem::enter_crop_mode", "Should not be called");
    }

    void copy_to_clipboard(QClipboard* clipboard)
    {
        clipboard->setText(this->toPlainText());
    }
};


#endif // MOVEITEM_H

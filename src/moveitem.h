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
#include <QImageReader>
#include <QMap>
#include <QMovie>
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

// Animated GIF, playable on the canvas (roadmap step 11). Inherits
// PixmapItem rather than building on ItemMixin directly - crop/scale/
// rotate/flip/opacity/color-gamut all keep working unmodified, since to
// Qt's rendering this is still "a QGraphicsPixmapItem with a pixmap set
// on it"; only WHICH pixmap is set changes, once per animation frame.
//
// Not a QObject (QGraphicsItem/QGraphicsPixmapItem aren't either) - the
// playback engine is QMovie, a real QObject we own and connect to via a
// context-less lambda connect() (safe: movie_ is destroyed together with
// this GifItem, in ~GifItem() below, which is the only thing that could
// ever stop it emitting).
class GifItem : public PixmapItem
{
public:
    const std::string TYPE = "gif"; // static constexpr

    explicit GifItem(const QByteArray& gifBytes,
                     const QString& filename = QString(),
                     QGraphicsPixmapItem* parent = nullptr)
        : PixmapItem(QImage(), filename, parent)
        , gifBytes_(gifBytes)
    {
        build_thumbnails_();
        init_movie_(); // connects frameChanged, jumps to frame 0
        // Base PixmapItem's ctor already ran reset_crop() against the
        // empty placeholder QImage() above - redo it now that pixmap()
        // reflects the real first frame's actual size.
        reset_crop();
        movie_->start();
        FLOG_DEBUG(familiar::log::Ch::Items, "Initialized {}", toString());
    }

    ~GifItem() override
    {
        delete movie_;
        delete gifBuffer_;
    }

    std::string get_type() const override { return TYPE; }

    QVariantMap get_extra_save_data() const override
    {
        QVariantMap data = PixmapItem::get_extra_save_data();
        data[QStringLiteral("speed")] = speed_percent();
        return data;
    }

    // Counterpart of get_extra_save_data()'s "speed" - called by
    // canvasscene.cpp after construction, same naming/shape as
    // TextItem::apply_extra_save_data().
    void apply_extra_save_data(const QVariantMap& data)
    {
        if (data.contains(QStringLiteral("speed")))
            set_speed_percent(data.value(QStringLiteral("speed")).toInt());
    }

    IBaseItem* create_copy() override
    {
        auto* item = new GifItem(gifBytes_, filename_);
        item->setPos(pos());
        item->setZValue(zValue());
        item->setScale(scale());
        item->setRotation(rotation());
        item->setOpacity(opacity());
        if (this->flip() == -1) {
            item->do_flip();
        }
        item->set_crop(crop_);
        item->set_speed_percent(speed_percent());
        return item;
    }

    // Exposed so GifPlaybackToolbar can connect directly to
    // frameChanged() itself (a real QObject signal) instead of polling -
    // same rationale as exposing document() from QGraphicsTextItem.
    QMovie* movie() const { return movie_; }
    const QByteArray& gif_bytes() const { return gifBytes_; }
    // Small pre-scaled thumbnails, one per frame, decoded once up front
    // for the "show all frames" filmstrip - QMovie::frameCount() is
    // lazily determined for some formats/plugins and not reliable before
    // a full pass, so this doubles as the authoritative frame count too.
    const QList<QPixmap>& frame_thumbnails() const { return frameThumbnails_; }
    int frame_count() const { return frameThumbnails_.size(); }

    bool is_playing() const
    {
        return movie_ && movie_->state() == QMovie::Running;
    }
    void play()
    {
        if (movie_)
            movie_->setPaused(false);
    }
    void pause()
    {
        if (movie_)
            movie_->setPaused(true);
    }
    void toggle_play_pause()
    {
        if (!movie_)
            return;
        if (is_playing())
            pause();
        else
            play();
    }
    int current_frame() const { return movie_ ? movie_->currentFrameNumber() : 0; }
    // Pauses (stepping/scrubbing while it's still playing would just get
    // immediately overridden by the next natural tick) and jumps by
    // `delta` frames, wrapping around either end.
    void step_frame(int delta)
    {
        if (!movie_ || frameThumbnails_.isEmpty())
            return;
        pause();
        const int count = frameThumbnails_.size();
        int next = (current_frame() + delta) % count;
        if (next < 0)
            next += count;
        movie_->jumpToFrame(next);
    }
    void jump_to_frame(int index)
    {
        if (!movie_ || frameThumbnails_.isEmpty())
            return;
        pause();
        movie_->jumpToFrame(qBound(0, index, frameThumbnails_.size() - 1));
    }
    // QMovie's own convention: 100 = normal speed, 25 = x0.25, 200 = x2 -
    // matches PureRef's playback-speed steps directly, no remapping.
    void set_speed_percent(int percent)
    {
        if (movie_)
            movie_->setSpeed(percent);
    }
    int speed_percent() const { return movie_ ? movie_->speed() : 100; }

private:
    // Decodes every frame once via QImageReader (independent of movie_ -
    // its own read position isn't disturbed) into small thumbnails for
    // the filmstrip. QImageReader::read()/canRead() auto-advance through
    // an animated source's frames, same idiom as Qt's own animated-image
    // examples - no explicit jumpToNextImage() needed.
    void build_thumbnails_()
    {
        QBuffer buf;
        buf.setData(gifBytes_);
        buf.open(QIODevice::ReadOnly);
        QImageReader reader(&buf);
        while (reader.canRead()) {
            QImage frame = reader.read();
            if (frame.isNull())
                break;
            frameThumbnails_.append(
                QPixmap::fromImage(frame).scaled(48,
                                                 48,
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
        }
    }

    void init_movie_()
    {
        gifBuffer_ = new QBuffer();
        gifBuffer_->setData(gifBytes_);
        gifBuffer_->open(QIODevice::ReadOnly);
        movie_ = new QMovie(gifBuffer_);
        movie_->setCacheMode(QMovie::CacheAll);
        // QObject::connect(), qualified: GifItem itself isn't a QObject
        // (QGraphicsItem/QGraphicsPixmapItem aren't either), so the bare
        // connect() free function - which only resolves via QObject's
        // OWN member/static lookup inside a QObject-derived class - isn't
        // in scope here. Calling the static method explicitly works
        // regardless of what class we're in, as long as the sender
        // (movie_) is a real QObject, which it is.
        QObject::connect(movie_, &QMovie::frameChanged, [this](int) {
            // QGraphicsPixmapItem::setPixmap(), NOT our own
            // PixmapItem::setPixmap() override - that one also calls
            // reset_crop(), which would wipe out any crop the user
            // applied, every single animation tick.
            this->QGraphicsPixmapItem::setPixmap(movie_->currentPixmap());
            this->update();
        });
        movie_->jumpToFrame(0);
    }

    QByteArray gifBytes_;
    QBuffer* gifBuffer_ = nullptr;
    QMovie* movie_ = nullptr;
    QList<QPixmap> frameThumbnails_;
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
            data[QStringLiteral("fill_color")] = fill_color_.name(
                QColor::HexArgb);
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

        const qreal clampedWidth = newWidth < 0 ? -1
                                                : qMax(newWidth, kMinFieldSize);
        const qreal clampedHeight = newHeight < 0
                                        ? -1
                                        : qMax(newHeight, kMinFieldSize);

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
            const QString href = document()->documentLayout()->anchorAt(
                event->pos());
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

// A persistent container (roadmap step 10) - unlike MultiSelectItem (an
// ephemeral overlay over CanvasScene::selectedItems(), owns nothing,
// never saved), a GroupItem is a real, saved item whose membership
// survives deselection and save/load. Members stay independent
// top-level scene items - nothing in this codebase uses real Qt
// scene-graph parenting (see MultiSelectItem/RubberbandItem, the same
// QGraphicsRectItem base this uses) - membership is tracked purely by
// uid, exactly as docs/fml_format_design.md's "future grouping" note
// anticipated. Stage 1 (this class): create/dissolve, move/resize/
// rotate as a unit. NOT yet done here: persistence (stage 2), lock and
// click-through-to-child dispatch (stage 3), the floating toolbar/fill
// picker (stage 4), drag-to-add and auto-expand (stage 5).
class GroupItem : public ItemMixin<GroupItem, QGraphicsRectItem>
{
public:
    const std::string TYPE = "group"; // static constexpr

    static QColor default_fill_color() { return QColor(20, 20, 20, 255); }
    // Visual breathing room kept between the members' own tight bounding
    // box and the group's fill rect - single source of truth for both
    // CanvasScene::group_selection() (initial fit, on creation) and
    // fit_to_contain_children() below (continuous refit).
    static constexpr qreal kPadding = 20.0;

    GroupItem(QGraphicsRectItem* parent = nullptr)
        : ItemMixin<GroupItem, QGraphicsRectItem>(parent)
        , fill_color_(default_fill_color())
    {
        init_selectable();
        // ItemPositionChange/ItemPositionHasChanged notifications are
        // OFF by default since Qt 4.6 (a performance opt-out most items
        // in this app never needed, since nothing else here overrides
        // itemChange() for geometry) - GroupItem is the first thing that
        // actually needs them, to drag its members along with it (see
        // itemChange() below). Only set here, not in the shared
        // init_selectable(), so every other item type's behavior/
        // performance stays exactly as before.
        this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        FLOG_DEBUG(familiar::log::Ch::Items, "Initialized {}", toString());
    }

    QString toString() const
    {
        return QString("Group (%1 items)").arg(childIds_.size());
    }

    bool is_image() const override { return false; }
    std::string get_type() const override { return TYPE; }
    bool is_editable() override { return false; }
    void enter_crop_mode() override
    {
        Q_ASSERT_X(false, "GroupItem::enter_crop_mode", "Should not be called");
    }

    IBaseItem* create_copy() override
    {
        // TODOLATER: copy/paste of a group doesn't yet duplicate its
        // members - pasting one produces an empty group with the same
        // fill/size/transform. Revisit once copy-paste of groups is
        // actually asked for; not implementing it here is deliberate,
        // not an oversight - groups are new enough (step 10) that this
        // gap is narrow (only reachable via Ctrl+C/V on a selected
        // group) and not worth guessing the right semantics for yet.
        auto* new_item = new GroupItem();
        new_item->set_local_rect(localRect_);
        new_item->fill_color_ = fill_color_;
        new_item->setPos(this->pos());
        new_item->setZValue(this->zValue());
        new_item->setScale(this->scale());
        new_item->setRotation(this->rotation());
        if (this->flip() == -1)
            new_item->do_flip();
        return new_item;
    }

    // Membership (child_ids) + fill color + the group's own local
    // footprint - everything x/y/z/scale/rotation/flip already covers
    // generically (CanvasScene::add_queued_items()) for every item type.
    // Put straight into manifest.json's per-item "data" object, the same
    // spot TextItem/GifItem stash their own type-specific payload in -
    // needs zero changes to ManifestItem/write_manifest()/parse_manifest()
    // (fml_archive.cpp), which already round-trip "data" as an opaque map.
    QVariantMap get_extra_save_data() const override
    {
        QVariantMap data;
        QVariantList idList;
        for (const QUuid& id : childIds_)
            idList.append(id.toString(QUuid::WithoutBraces));
        data[QStringLiteral("child_ids")] = idList;
        if (fill_color_ != default_fill_color())
            data[QStringLiteral("fill_color")] = fill_color_.name(
                QColor::HexArgb);
        data[QStringLiteral("rect_width")] = localRect_.width();
        data[QStringLiteral("rect_height")] = localRect_.height();
        return data;
    }

    // Counterpart of get_extra_save_data() - child_ids are set as-is
    // (raw uids, not yet resolved to live pointers); resolve_children()
    // resolves them lazily against the scene on first actual use, by
    // which point every item in this load batch already exists (see
    // CanvasScene::add_queued_items()'s own belt-and-suspenders
    // invalidate_children_cache() pass, for why "lazily" is safe here
    // despite manifest.json having no parent-before/after-child
    // ordering guarantee).
    void apply_extra_save_data(const QVariantMap& data)
    {
        QList<QUuid> ids;
        for (const QVariant& v :
             data.value(QStringLiteral("child_ids")).toList()) {
            const QUuid id = QUuid::fromString(v.toString());
            if (!id.isNull())
                ids.append(id);
        }
        set_child_ids(ids);

        const QString fill = data.value(QStringLiteral("fill_color"))
                                 .toString();
        if (!fill.isEmpty()) {
            QColor c(fill);
            if (c.isValid())
                fill_color_ = c;
        }

        const qreal w = data.value(QStringLiteral("rect_width"), 0.0)
                            .toReal();
        const qreal h = data.value(QStringLiteral("rect_height"), 0.0)
                            .toReal();
        if (w > 0 && h > 0)
            set_local_rect(QRectF(0, 0, w, h));
    }

    // See apply_extra_save_data()'s comment - the safety net
    // CanvasScene::add_queued_items() invokes after a whole load batch.
    void invalidate_children_cache() { resolvedChildrenDirty_ = true; }

    QColor fill_color() const { return fill_color_; }
    void set_fill_color(const QColor& color)
    {
        fill_color_ = color;
        update();
    }

    QRectF bounding_rect_unselected() const override { return localRect_; }

    // Sets the group's own local footprint - called once at creation
    // (fit to the union of the items being grouped, see
    // CanvasScene::group_selection()) and later by auto-expand (step 10
    // stage 5) when a member is dragged outside it.
    void set_local_rect(const QRectF& rect)
    {
        this->prepareGeometryChange();
        localRect_ = rect;
        this->setRect(rect);
    }

    // Fits (grows OR shrinks) the group's footprint to tightly contain
    // every member plus kPadding, called from CanvasScene::on_change() -
    // same hook MultiSelectItem's own live refit already uses, so this
    // responds during an active drag, not just after release. Recomputed
    // from the members' OWN current positions every time (not seeded
    // from the group's existing rect) - seeding from the existing rect
    // would only ever let it grow, since united() can't shrink past
    // whatever it started from; a member dragged back toward the others
    // should let the group shrink back down too, not stay stretched from
    // wherever it had been. No-op if already an exact fit (avoids an
    // infinite setPos()/set_local_rect() -> changed() -> on_change()
    // loop; it only recurses the one extra time needed to notice
    // "already settled").
    // TODOLATER: computed via plain sceneBoundingRect() union, which
    // doesn't account for the GROUP's own rotation (fine for now - axis-
    // aligned groups are the only case exercised so far); a rotated
    // group would need the same corner-projection approach
    // CanvasScene::itemsBoundingRect() uses.
    void fit_to_contain_children()
    {
        const QList<QGraphicsItem*> children = resolve_children();
        if (children.isEmpty())
            return;

        QRectF unionRect = children.first()->sceneBoundingRect();
        for (QGraphicsItem* child : children)
            unionRect = unionRect.united(child->sceneBoundingRect());
        unionRect = unionRect.adjusted(-kPadding, -kPadding, kPadding, kPadding);

        // mapRectToScene(localRect_), NOT sceneBoundingRect() - the
        // latter goes through SelectableMixin's boundingRect() override,
        // which pads in extra room for the selection handles whenever
        // the group is selected (practically always, right after
        // creating or interacting with one). Comparing against it would
        // bake that transient decoration margin into localRect_
        // permanently on every single refit.
        const QRectF ownSceneRect = this->mapRectToScene(localRect_);
        if (unionRect == ownSceneRect)
            return;

        // autoExpanding_ tells itemChange() below not to treat this
        // setPos() as a body drag needing to carry members along - the
        // members are exactly why we're repositioning in the first
        // place (one of them moved outside/back-inside the old rect);
        // shifting them AGAIN by the group's own repositioning delta
        // would compound that move instead of just visually catching up
        // to it.
        autoExpanding_ = true;
        this->setPos(unionRect.topLeft());
        autoExpanding_ = false;
        set_local_rect(QRectF(0, 0, unionRect.width(), unionRect.height()));
    }

    // ── Membership (uid-based - see class comment) ─────────────────────
    const QList<QUuid>& child_ids() const { return childIds_; }
    void set_child_ids(const QList<QUuid>& ids)
    {
        childIds_ = ids;
        resolvedChildrenDirty_ = true;
    }
    void add_child_id(const QUuid& id)
    {
        if (!childIds_.contains(id))
            childIds_.append(id);
        resolvedChildrenDirty_ = true;
    }
    void remove_child_id(const QUuid& id)
    {
        childIds_.removeAll(id);
        resolvedChildrenDirty_ = true;
    }

    // Live QGraphicsItem*s for child_ids(), resolved against the scene
    // and cached until membership changes - re-resolving via a linear
    // scan on every mouse-move of an active drag would be wasteful. A
    // member that's been deleted out from under the group (dangling
    // uid) is silently skipped, not treated as an error - TODOLATER:
    // groups don't yet react to a member's own deletion by pruning
    // child_ids(), so a deleted member's uid lingers (harmlessly - this
    // already skips it) until the group is next saved and reloaded.
    QList<QGraphicsItem*> resolve_children()
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (!scene)
            return {};
        if (resolvedChildrenDirty_) {
            resolvedChildren_.clear();
            for (const QUuid& id : childIds_) {
                if (QGraphicsItem* item = scene->find_by_uid(id))
                    resolvedChildren_.append(item);
            }
            resolvedChildrenDirty_ = false;
        }
        return resolvedChildren_;
    }

    // Drives the standard resize/rotate handles (SelectableMixin,
    // selector.h) to scale/rotate the group's own fill rect AND every
    // member together, each independently anchored - the same mechanism
    // MultiSelectItem's own selection_action_items() already uses for an
    // ephemeral multi-selection, just with the fill rect itself (`this`)
    // included this time, since GroupItem (unlike MultiSelectItem) is
    // the persistent, always-visible container - see itemChange() below
    // for why including `this` here doesn't double-move members during
    // an active handle drag.
    QList<QGraphicsItem*> selection_action_items() override
    {
        QList<QGraphicsItem*> items;
        items << this;
        items << resolve_children();
        return items;
    }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(fill_color_));
        painter->drawRect(bounding_rect_unselected());
        this->paint_selectable(painter, option, widget);
    }

    // A group must always sit BEHIND its members (it's a background
    // fill, see GroupCommand's z-value handling in commands.cpp) -
    // letting the stock bring-to-front-on-select behavior
    // (ItemMixin::on_selected_change()) run for a GroupItem the same as
    // for any other item would cover its own children with the fill the
    // instant it's clicked. Groups don't participate in that per-item
    // z-reordering at all (same reasoning as MultiSelectItem's identical
    // no-op override, selector.h) - raising a selected group's whole
    // cluster (fill + members) together is instead handled ONE level up,
    // generically, by CanvasScene::raise_selection_to_front() (which
    // expands a selected GroupItem into group+members specifically so
    // this doesn't need its own copy of that logic here).
    void on_selected_change(bool value) override { Q_UNUSED(value) }

protected:
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant& value) override
    {
        // Only for a plain Qt-native body drag (ItemIsMovable) - during
        // an active resize/rotate handle drag, active_mode_ is already
        // kScaleMode/kRotateMode and every member already gets its OWN
        // independent set_scale()/set_rotation() call in the very same
        // loop (see selection_action_items() above feeding
        // SelectableMixin's handle code, selector.h). Shifting members
        // again here off this item's own incidental setPos() (part of
        // that same anchor-compensation math) would double-apply the
        // movement - active_mode_ == kNone is exactly "not mid-handle-
        // drag", i.e. a real independent body drag of the group.
        // !autoExpanding_ - see fit_to_contain_children() above: that
        // method's own setPos() is repositioning the group TO catch up
        // with a member that already moved, not a body drag that should
        // carry members along.
        if (change == QGraphicsItem::ItemPositionChange && this->scene()
            && active_mode_ == kNone && !autoExpanding_) {
            const QPointF delta = value.toPointF() - this->pos();
            if (!delta.isNull()) {
                for (QGraphicsItem* child : resolve_children())
                    child->moveBy(delta.x(), delta.y());
            }
        }
        // NOT QGraphicsRectItem::itemChange() directly - SelectableMixin
        // has its own itemChange() override (selector.h) that reacts to
        // ItemSelectedChange (bring-to-front-on-select/push-behind-on-
        // deselect, via ItemMixin::on_selected_change()); calling the Qt
        // base directly here would silently skip that for every group.
        return SelectableMixin<GroupItem, QGraphicsRectItem>::itemChange(
            change, value);
    }

private:
    QRectF localRect_;
    QColor fill_color_;
    QList<QUuid> childIds_;
    QList<QGraphicsItem*> resolvedChildren_;
    bool resolvedChildrenDirty_ = true;
    bool autoExpanding_ = false;
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

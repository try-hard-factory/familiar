#ifndef MOVEITEM_H
#define MOVEITEM_H

#include "commands.h"
#include "core/settings.h"
#include "core/settingshandler.h"
#include "selector.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QDebug>
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
#include <QVariantMap>
#include <QWheelEvent>
#include <QtGlobal>
#include <qdebug.h>

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

    virtual bool has_selection_handles() const
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        return this->isSelected() && scene && scene->has_single_selection();
    }

    virtual QList<QGraphicsItem*> selection_action_items()
    {
        return QList<QGraphicsItem*>() << this;
    }


    virtual void on_selected_change(bool value)
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        // Only bring to front when the user directly clicked this item
        // (kMoveMode). During a rubber-band drag (kRubberbandMode),
        // stacked/overlapping images would otherwise get reshuffled in
        // z-order just by being swept over, which is surprising and
        // unwanted - selecting shouldn't itself change stacking order.
        if (value && scene && !scene->has_selection()
            && scene->active_mode() == CanvasScene::ESceneMode::kMoveMode) {
            this->bring_to_front();
        }
    }

    void update_from_data()
    {
        // TODOLATER:
    }
    void unset_cursor()
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        // TODOLATER:
        emit scene->cursor_cleared();
    }
};

class PixmapItem : public ItemMixin<PixmapItem, QGraphicsPixmapItem>
{
public:
    const std::string TYPE = "pixmap"; // static constexpr
    const qreal CROP_HANDLE_SIZE = 15; // static constexpr
    using ColorGamut = QMap<QPair<int, int>, int>;
    using CropHandleFn = QRectF (PixmapItem::*)() const;
    std::optional<int> save_id{};
    QString filename_;
    bool is_image_{true};
    bool crop_mode = false;
    bool grayscale_ = false;
    QPixmap grayscalePixmap_{};
    mutable std::optional<ColorGamut> colorGamut_{};
    FamSettings* settings{nullptr};

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
        qDebug() << "Initialized " << toString();
        // save_id = nullptr;
        crop_mode = false;
        init_selectable();
        settings = FamSettings::getInstance();
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
        qDebug() << "Setting crop for " << toString() << " to " << crop;
        this->prepareGeometryChange();
        this->crop_ = crop;
        this->update();
    }

    bool grayscale() const { return grayscale_; }
    void setGrayscale(bool value)
    {
        qDebug() << "Setting grayscale for " << toString() << " to " << value;
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

    QVariantMap get_extra_save_data() const
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

    // TODOLATER
    QString get_filename_for_export(const QString& imgformat,
                                    std::optional<int> save_id_default
                                    = std::nullopt) const
    {
        std::optional<int> id = save_id ? save_id : save_id_default;
        Q_ASSERT(id.has_value());

        if (!filename_.isEmpty()) {
            QString basename = QFileInfo(filename_).completeBaseName();
            return QString("%1-%2.%3")
                .arg(*id, 4, 10, QChar('0'))
                .arg(basename, imgformat);
        }
        return QString("%1.%2").arg(*id, 4, 10, QChar('0')).arg(imgformat);
    }

    // Determines the format for storing this image.
    QString get_imgformat(const QImage& img) const
    {
        QString formt = settings
                            ->valueOrDefault(
                                QStringLiteral("Items/image_storage_format"))
                            .toString();

        if (formt == QLatin1String("best")) {
            if (img.hasAlphaChannel()
                || (img.height() < 500 && img.width() < 500)) {
                formt = QStringLiteral("png");
            } else {
                formt = QStringLiteral("jpg");
            }
        }

        qDebug() << "Found format " << formt << " for " << toString();
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
            qDebug() << "Calculating color gamut for " << toString();
            ColorGamut gamut;
            QImage img = pixmap().toImage();
            // Don't evaluate every pixel for larger images:
            int step = std::max(1,
                                static_cast<int>(
                                    std::max(img.width(), img.height()) / 1000));
            qDebug() << "Considering every " << step << ". row/column";

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

            qDebug() << "Got " << gamut.size() << " color gamut values";
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
        qDebug() << "Entering crop mode on " << toString();
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
        qDebug() << "Exiting crop mode with " << confirm << " on "
                 << toString();
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
            QGraphicsItem::hoverMoveEvent(event);
            return;
        }

        for (auto handle : crop_handles()) {
            if ((this->*handle)().contains(event->pos())) {
                setCursor(get_crop_handle_cursor(handle));
                return;
            }
        }

        for (auto edge : crop_edges()) {
            if ((this->*edge)().contains(event->pos())) {
                setCursor(get_crop_edge_cursor(edge));
                return;
            }
        }

        unsetCursor();
        // setCursor(Qt::ArrowCursor);
        // unset_cursor();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (!crop_mode) {
            QGraphicsItem::mousePressEvent(event);
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
            QGraphicsPixmapItem::mouseMoveEvent(event);
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (crop_mode) {
            crop_mode_move = std::nullopt;
            crop_mode_event_start = std::nullopt;
            event->accept();
        } else {
            QGraphicsPixmapItem::mouseReleaseEvent(event);
        }
    }
};

class TextItem : public ItemMixin<TextItem, QGraphicsTextItem>
{
public:
    const std::string TYPE = "text"; // static constexpr
    bool edit_mode = false;
    QString old_text;
    std::optional<int> save_id{};

    TextItem(const QString& text = QString(), QGraphicsTextItem* parent = nullptr)
        : ItemMixin<TextItem, QGraphicsTextItem>(parent)
    {
        setPlainText(text.isEmpty() ? QStringLiteral("Text") : text);
        
        init_selectable();
        edit_mode = false;
        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        setDefaultTextColor(colorPreset[EPresetsColorIdx::kTextColor]);
        qDebug() << "Initialized " << toString();
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
        return new TextItem(data.value(QStringLiteral("text")).toString());
    }

    QVariantMap get_extra_save_data() const
    {
        QVariantMap data;
        data[QStringLiteral("text")] = this->toPlainText();
        return data;
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
        QColor color(0, 0, 0);
        color.setAlpha(40);
        painter->setBrush(QBrush(color));
        painter->drawRect(QGraphicsTextItem::boundingRect());
        QStyleOptionGraphicsItem updatedOption(*option);
        updatedOption.state |= QStyle::State_Enabled;
        QGraphicsTextItem::paint(painter, &updatedOption, widget);
        this->paint_selectable(painter, option, widget);
    }

    IBaseItem* create_copy() override
    {
        auto* new_item = new TextItem(this->toPlainText());
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
        qDebug() << "Entering edit mode on " << toString();
        edit_mode = true;
        old_text = this->toPlainText();
        this->setTextInteractionFlags(Qt::TextEditorInteraction);
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->edit_item = this;
    }

    void exit_edit_mode(bool commit = true)
    {
        qDebug() << "Exiting edit mode on " << toString();
        edit_mode = false;
        // Reset selection:
        this->setTextCursor(QTextCursor(document()));
        this->setTextInteractionFlags(Qt::NoTextInteraction);
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->edit_item = nullptr;
        if (commit) {
            scene->undo_stack_->push(
                new ChangeTextCommand(this, this->toPlainText(), old_text));
            if (this->toPlainText().trimmed().isEmpty()) {
                qDebug() << "Removing empty text item";
                scene->undo_stack_->push(
                    new DeleteItemsCommand(scene, QList<QGraphicsItem*>{this}));
            }
        } else {
            setPlainText(old_text);
        }
    }

    bool has_selection_handles() const override
    {
        return ItemMixin<TextItem, QGraphicsTextItem>::has_selection_handles()
               && !edit_mode;
    }

    void copy_to_clipboard(QClipboard* clipboard)
    {
        clipboard->setText(this->toPlainText());
    }
    void enter_crop_mode() override
    {
        Q_ASSERT_X(false, "TextItem::enter_crop_mode", "Should not be called");
    }


protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && event->modifiers() == Qt::NoModifier) {
            exit_edit_mode();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape
            && event->modifiers() == Qt::NoModifier) {
            exit_edit_mode(false);
            event->accept();
            return;
        }
        QGraphicsTextItem::keyPressEvent(event);
    }


};

// Displayed instead of an item that couldn't be loaded from a save file.
// Won't itself be saved; the original item's data is preserved unless this
// stand-in gets deleted or the file is saved again.
class ErrorItem : public ItemMixin<ErrorItem, QGraphicsTextItem>
{
public:
    const std::string TYPE = "error"; // static constexpr
    std::optional<int> original_save_id{};

    ErrorItem(const QString& text = QString(),
              QGraphicsTextItem* parent = nullptr)
        : ItemMixin<ErrorItem, QGraphicsTextItem>(parent)
    {
        setPlainText(text.isEmpty() ? QStringLiteral("Text") : text);
        init_selectable();
        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        setDefaultTextColor(colorPreset[EPresetsColorIdx::kTextColor]);
        qDebug() << "Initialized " << toString();
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
        updatedOption.state |= QStyle::State_Enabled;
        QGraphicsTextItem::paint(painter, &updatedOption, widget);
        this->paint_selectable(painter, option, widget);
    }

    void update_from_data()
    {
        // TODOLATER: kwargs-driven data loading isn't wired up yet;
        // Python sets original_save_id/pos/z/scale/rotation from the
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

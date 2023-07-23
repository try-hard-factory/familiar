#ifndef MOVEITEM_H
#define MOVEITEM_H

#include "selector.h"
#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTextCursor>
#include <QWheelEvent>
#include <QtGlobal>

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

    // slot?
    virtual void on_selected_change(bool value)
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        if (value && scene && !scene->has_selection() && !scene->rubberband_active) {
            this->bring_to_front();
        }
    }

    void update_from_data() {}
};

class PixmapItem : public ItemMixin<PixmapItem, QGraphicsPixmapItem>
{
public:
    bool is_croppable = true;
    bool crop_mode = false;
    QRectF crop_{};
    QRectF crop_temp{};
    QPointF crop_mode_event_start{};
    QRectF crop_mode_move{};
    const qreal CROP_HANDLE_SIZE = 15;
    PixmapItem(QGraphicsPixmapItem* parent = nullptr)
        : ItemMixin<PixmapItem, QGraphicsPixmapItem>(parent)
    {
        // save_id = nullptr;
        reset_crop();
        is_croppable = true;
        crop_mode = false;
        init_selectable();
    }

    // set_image function

    int type() const override { return 777; }
    // static PixmapItem* create_from_data() {}

    QRectF crop() { return crop_; }
    void set_crop(const QRectF& crop)
    {
        this->prepareGeometryChange();
        this->crop_ = crop;
        this->update();
    }

    QRectF bounding_rect_unselected() const override
    {
        if (crop_mode) {
            //ItemMixin<PixmapItem, QGraphicsPixmapItem>::bounding_rect_unselected();
            return QGraphicsPixmapItem::boundingRect();
        }

        return crop_;
    }

    QByteArray pixmapToBytes()
    {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        auto img = this->pixmap().toImage();
        img.save(&buffer, "PNG");
        return byteArray;
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

    PixmapItem* create_copy()
    {
        PixmapItem* item = new PixmapItem(nullptr);
        item->setPixmap(pixmap());
        item->setPos(pos());
        item->setZValue(zValue());
        item->setScale(scale());
        item->setRotation(rotation());
        if (flip() == -1) {
            item->do_flip();
        }
        item->set_crop(crop_);
        return item;
    }

    void copy_to_clipboard(QClipboard* clipboard) { clipboard->setPixmap(this->pixmap()); }

    void reset_crop()
    {
        crop_ = QRectF(0, 0, this->pixmap().size().width(), this->pixmap().size().height());
    }

    qreal crop_handle_size() const { return this->fixed_length_for_viewport(CROP_HANDLE_SIZE); }

    QRectF crop_handle_topleft() const
    {
        QPointF topLeft = crop_temp.topLeft();
        return QRectF(topLeft.x(), topLeft.y(), crop_handle_size(), crop_handle_size());
    }

    QRectF crop_handle_bottomleft() const
    {
        QPointF bottomLeft = crop_temp.bottomLeft();
        return QRectF(bottomLeft.x(),
                      bottomLeft.y() - crop_handle_size(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_handle_bottomright() const
    {
        QPointF bottomRight = crop_temp.bottomRight();
        return QRectF(bottomRight.x() - crop_handle_size(),
                      bottomRight.y() - crop_handle_size(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_handle_topright() const
    {
        QPointF topRight = crop_temp.topRight();
        return QRectF(topRight.x() - crop_handle_size(),
                      topRight.y(),
                      crop_handle_size(),
                      crop_handle_size());
    }

    // The other crop handle functions...

    // Function to return all crop handle functions as a tuple
    QList<QRectF> crop_handles() const
    {
        QList<QRectF> handles;
        handles << crop_handle_topleft();
        handles << crop_handle_bottomleft();
        handles << crop_handle_bottomright();
        handles << crop_handle_topright();
        return handles;
    }

    QRectF crop_edge_top() const
    {
        QPointF topLeft = crop_temp.topLeft();
        return QRectF(topLeft.x() + crop_handle_size(),
                      topLeft.y(),
                      crop_temp.width() - 2 * crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_edge_left() const
    {
        QPointF topLeft = crop_temp.topLeft();
        return QRectF(topLeft.x(),
                      topLeft.y() + crop_handle_size(),
                      crop_handle_size(),
                      crop_temp.height() - 2 * crop_handle_size());
    }

    QRectF crop_edge_bottom() const
    {
        QPointF bottomLeft = crop_temp.bottomLeft();
        return QRectF(bottomLeft.x() + crop_handle_size(),
                      bottomLeft.y() - crop_handle_size(),
                      crop_temp.width() - 2 * crop_handle_size(),
                      crop_handle_size());
    }

    QRectF crop_edge_right() const
    {
        QPointF topRight = crop_temp.topRight();
        return QRectF(topRight.x() - crop_handle_size(),
                      topRight.y() + crop_handle_size(),
                      crop_handle_size(),
                      crop_temp.height() - 2 * crop_handle_size());
    }

    // Function to return all crop edge functions as a tuple
    QList<QRectF> crop_edges() const
    {
        QList<QRectF> handles;
        handles << crop_edge_top();
        handles << crop_edge_left();
        handles << crop_edge_bottom();
        handles << crop_edge_right();
        return handles;
    }

    Qt::CursorShape get_crop_handle_cursor(const QRectF& handle)
    {
        bool is_topleft_or_bottomright = (handle == crop_handle_topleft()
                                          || handle == crop_handle_bottomright());
        return get_diag_cursor(is_topleft_or_bottomright);
    }

    Qt::CursorShape get_crop_edge_cursor(const QRectF& edge)
    {
        // Определяем значение top_or_bottom с помощью условного оператора
        bool top_or_bottom = (edge == crop_edge_top() || edge == crop_edge_bottom());

        // Определяем значение sideways с помощью условного оператора
        bool sideways = (45 < rotation() && rotation() < 135)
                        || (225 < rotation() && rotation() < 315);

        // Используем условный оператор для определения типа курсора и возвращаем его
        return (top_or_bottom == sideways) ? Qt::SizeHorCursor : Qt::SizeVerCursor;
    }

    void draw_crop_rect(QPainter& painter, const QRectF& rect)
    {
        QPen pen(Qt::white); // Устанавливаем белый цвет пера
        pen.setWidth(2);     // Устанавливаем толщину пера равной 2
        pen.setCosmetic(
            true); // Устанавливаем косметический режим, чтобы толщина пера не зависела от масштаба

        painter.setPen(pen);
        painter.drawRect(rect);

        // Создаем пунктирный стиль пера
        pen.setColor(Qt::black); // Задаем черный цвет для пунктирной линии
        pen.setStyle(Qt::DotLine); // Устанавливаем стиль пунктирной линии

        painter.setPen(pen);
        painter.drawRect(rect);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        if (crop_mode) {
            // paint_debug(painter, option, widget);

            // Darken image outside of cropped area
            painter->drawPixmap(0, 0, pixmap());
            QPainterPath path;
            path.addRect(crop_temp);
            QColor color(0, 0, 0);
            color.setAlpha(100);
            painter->setBrush(QBrush(color));
            painter->setPen(Qt::NoPen);
            painter->drawPath(path);
            painter->setBrush(Qt::NoBrush);

            // Draw crop handles
            for (const auto& handle : crop_handles()) {
                draw_crop_rect(*painter, handle);
            }

            // Draw crop rectangle
            draw_crop_rect(*painter, crop_temp);
        } else {
            painter->drawPixmap(crop_, pixmap(), crop_);
            paint_selectable(painter, option, widget);
        }
    }

    void enter_crop_mode()
    {
        this->prepareGeometryChange();
        crop_mode = true;
        crop_temp = crop();
        crop_mode_move = QRectF();
        crop_mode_event_start = QPointF();
        this->grabKeyboard();
        this->update();
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->crop_item = this;
    }

    void exit_crop_mode(bool confirm)
    {
        if (confirm && crop() != crop_temp) {
            // self.scene().undo_stack.push(commands.CropItem(self, self.crop_temp))
        }

        this->prepareGeometryChange();
        crop_mode = false;
        crop_temp = QRectF();
        crop_mode_move = QRectF();
        crop_mode_event_start = QPointF();
        this->ungrabKeyboard();
        this->update();
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        scene->crop_item = nullptr;
    }

    QPoint ensure_point_within_pixmap_bounds(const QPointF& point)
    {
        // Assuming `pixmap_` is the member variable containing the pixmap
        int maxX = pixmap().width();  // - 1;
        int maxY = pixmap().height(); // - 1;

        int x = qBound((qreal) 0, point.x(), (qreal) maxX);
        int y = qBound((qreal) 0, point.y(), (qreal) maxY);
        //point.setX(min(self.pixmap().size().width(), max(0, point.x())))
        //point.setY(min(self.pixmap().size().height(), max(0, point.y())))

        return QPoint(x, y);
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

        for (const auto& handle : crop_handles()) {
            if (handle.contains(event->pos())) {
                setCursor(get_crop_handle_cursor(handle));
                return;
            }
        }

        for (const auto& edge : crop_edges()) {
            if (edge.contains(event->pos())) {
                setCursor(get_crop_edge_cursor(edge));
                return;
            }
        }

        setCursor(Qt::ArrowCursor);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (!crop_mode) {
            QGraphicsItem::mousePressEvent(event);
            return;
        }

        event->accept();

        for (const auto& handle : crop_handles()) {
            // Click into a handle?
            if (handle.contains(event->pos())) {
                crop_mode_event_start = event->pos();
                crop_mode_move = handle;
                return;
            }
        }

        for (const auto& edge : crop_edges()) {
            // Click into an edge handle?
            if (edge.contains(event->pos())) {
                crop_mode_event_start = event->pos();
                crop_mode_move = edge;
                return;
            }
        }

        // Click not in handle, end cropping mode:
        exit_crop_mode(crop_temp.contains(event->pos()));
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (crop_mode) {
            QPointF diff = event->pos() - crop_mode_event_start;

            if (crop_mode_move == crop_handle_topleft()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.topLeft() + diff);
                crop_temp.setTopLeft(newPoint);
            } else if (crop_mode_move == crop_handle_bottomleft()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.bottomLeft() + diff);
                crop_temp.setBottomLeft(newPoint);
            } else if (crop_mode_move == crop_handle_bottomright()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.bottomRight() + diff);
                crop_temp.setBottomRight(newPoint);
            } else if (crop_mode_move == crop_handle_topright()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.topRight() + diff);
                crop_temp.setTopRight(newPoint);
            } else if (crop_mode_move == crop_edge_top()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.topLeft() + diff);
                crop_temp.setTop(newPoint.y());
            } else if (crop_mode_move == crop_edge_left()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.topLeft() + diff);
                crop_temp.setLeft(newPoint.x());
            } else if (crop_mode_move == crop_edge_bottom()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.bottomLeft() + diff);
                crop_temp.setBottom(newPoint.y());
            } else if (crop_mode_move == crop_edge_right()) {
                QPointF newPoint = ensure_point_within_pixmap_bounds(crop_temp.topRight() + diff);
                crop_temp.setRight(newPoint.x());
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
            crop_mode_move = QRectF();
            crop_mode_event_start = QPointF();
            event->accept();
        } else {
            QGraphicsPixmapItem::mouseReleaseEvent(event);
        }
    }
};

class TextItem : public ItemMixin<TextItem, QGraphicsTextItem>
{
public:
    TextItem(QGraphicsTextItem* parent = nullptr)
        : ItemMixin<TextItem, QGraphicsTextItem>(parent)
    {
        // save_id = nullptr;
        is_croppable = false;
        init_selectable();
        is_editable = true;
        edit_mode = false;
        setDefaultTextColor(QColor(0, 0, 0, 128));
    }

    int type() const override { return 666; }

    void set_text(QString text) { this->setPlainText(text); }
    void set_pos_center(const QPointF& pos) override
    {
        setPos(pos - QPointF(boundingRect().width() / 2.0, boundingRect().height() / 2.0));
    }

    bool has_selection_outline() const override { return isSelected(); }

    bool has_selection_handles() const override { return ItemMixin::has_selection_handles(); }

    static TextItem* create_from_data() { return nullptr; }

    QString get_extra_save_data() { return this->toPlainText(); }

    bool contains(const QPointF& point) { return this->boundingRect().contains(point); }

    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override
    {
        painter->setPen(Qt::NoPen);
        auto color = QColor(0, 0, 0, 0);
        auto brush = QBrush(color);
        painter->setBrush(brush);
        painter->drawRect(boundingRect());
        QStyleOptionGraphicsItem updatedOption(*option);
        updatedOption.state |= QStyle::State_Enabled;
        QGraphicsTextItem::paint(painter, &updatedOption, widget);
        this->paint_selectable(painter, option, widget);
    }

    TextItem* create_copy()
    {
        auto* new_item = new TextItem();
        new_item->setPlainText(this->get_extra_save_data());
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
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        edit_mode = true;
        this->setTextInteractionFlags(Qt::TextEditorInteraction);
        scene->edit_item = this;
    }

    void exit_edit_mode()
    {
        auto* scene = dynamic_cast<CanvasScene*>(this->scene());
        edit_mode = false;
        // Reset selection:
        this->setTextCursor(QTextCursor(document()));
        this->setTextInteractionFlags(Qt::NoTextInteraction);
        scene->edit_item = nullptr;
    }


    void copy_to_clipboard(QClipboard* clipboard)
    {
        clipboard->setText(this->get_extra_save_data());
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Return && event->modifiers() == Qt::NoModifier) {
            auto* scene = dynamic_cast<CanvasScene*>(this->scene());
            exit_edit_mode();
            scene->edit_item = nullptr;
            event->accept();
            return;
        }
        QGraphicsTextItem::keyPressEvent(event);
    }

public:
    bool is_croppable = false;
    bool is_editable = true;
    bool edit_mode = false;
};


class MoveItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MoveItem(const QString& path, uint64_t& zc, QGraphicsRectItem* parent = nullptr);
    MoveItem(QImage* img, uint64_t& zc, QGraphicsRectItem* parent = nullptr);
    MoveItem(QByteArray ba,
             int w,
             int h,
             qsizetype bpl,
             QImage::Format f,
             uint64_t& zc,
             QGraphicsRectItem* parent = nullptr);
    ~MoveItem();
    QRectF getRect() const { return boundingRect(); }
    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF& rect);
    void setInGroup(bool f) { inGroup_ = f; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    int height() const noexcept { return qimage_->height(); }
    int width() const noexcept { return qimage_->width(); }
    QImage::Format format() const noexcept { return qimage_->format(); }
    const QImage& qimage() { return *qimage_; }
    const QImage* qimage_ptr_const() { return qimage_; }
    QImage* qimage_ptr() { return qimage_; }
    uint64_t& zcounter() noexcept { return zCounter_; }

public slots:
    void settingsChangedSlot();

signals:
protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    //    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QPointF shiftMouseCoords_;
    QImage* qimage_;
    QPixmap pixmap_;
    QByteArray ba_;
    uint64_t& zCounter_;
    QSizeF _size;
    QRectF rect_;
    bool inGroup_ = false;
    QColor selectionColor_;
    int currentOpacity_;
};

#endif // MOVEITEM_H

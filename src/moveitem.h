#ifndef MOVEITEM_H
#define MOVEITEM_H

#include "selector.h"
#include <QCursor>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>
#include <QTextCursor>
#include <QClipboard>

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
{};

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

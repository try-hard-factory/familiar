#ifndef MOVEITEM_H
#define MOVEITEM_H

#include <QCursor>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

class MoveItemBase : public QObject
{
    Q_OBJECT
public:

};

class MoveItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MoveItem(const QString& path,
             uint64_t& zc,
             QGraphicsRectItem* parent = nullptr);
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
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
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

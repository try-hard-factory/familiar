#ifndef MOVEITEM_H
#define MOVEITEM_H

#include <QObject>
#include <QWheelEvent>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QCursor>

class MoveItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MoveItem(const QString& path, uint64_t& zc, QGraphicsRectItem *parent = nullptr);
    MoveItem(const QImage& img, uint64_t& zc, QGraphicsRectItem *parent = nullptr);
    ~MoveItem();
    QRectF getRect() const { return boundingRect(); }
    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF &rect);
    void setInGroup(bool f) { inGroup_ = f; }
    QRectF boundingRect() const override;
    int height() const noexcept { return qimage_.height(); }
    int width() const noexcept { return qimage_.width(); }
    QImage::Format format() const noexcept { return qimage_.format(); }
    const QImage& qimage() { return qimage_; }
signals:
protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
//    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
//    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
//    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
//    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QPointF shiftMouseCoords_;
    QImage qimage_;
    QPixmap pixmap_;
    uint64_t& zCounter_;
    QSizeF _size;
    QRectF rect_;
    bool inGroup_ = false;
public slots:
};

#endif // MOVEITEM_H

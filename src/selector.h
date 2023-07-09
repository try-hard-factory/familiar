#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QDebug>

class BaseItemMixin : public QGraphicsItem {
public:
    BaseItemMixin(QGraphicsItem* parent = nullptr) : QGraphicsItem(parent) {}

    void setScale(qreal value, const QPointF& anchor = QPointF(0, 0)) {
        if (value <= 0) {
            return;
        }

        qDebug() << "Setting scale to" << value;
        prepareGeometryChange();
        QPointF prev = mapToScene(anchor);
        QGraphicsItem::setScale(value);
        QPointF diff = mapToScene(anchor) - prev;
        setPos(pos() - diff);
    }

    void setZValue(qreal value) {
        qDebug() << "Setting z-value to" << value;
        QGraphicsItem::setZValue(value);
        if (scene()) {
            scene()->max_z = qMax(scene()->max_z, value);
            scene()->min_z = qMin(scene()->min_z, value);
        }
    }

    void bringToFront() {
        setZValue(scene()->max_z + scene()->Z_STEP);
    }

    void setRotation(qreal value, const QPointF& anchor = QPointF(0, 0)) {
        qDebug() << "Setting rotation to" << value;
        QGraphicsItem::setRotation(value % 360);
        QPointF prev = mapToScene(anchor);
        QPointF diff = mapToScene(anchor) - prev;
        setPos(pos() - diff);
    }

    qreal flip() {
        // We use the transformation matrix only for flipping, so checking
        // the x scale is enough
        return transform().m11();
    }

    void doFlip(bool vertical = false, const QPointF& anchor = QPointF(0, 0)) {
        setTransform(QTransform::fromScale(-flip(), 1));
        if (vertical) {
            setRotation(rotation() + 180, anchor);
        }
    }

    QRectF boundingRectUnselected() const {
        return QGraphicsItem::boundingRect();
    }

    qreal width() const {
        return boundingRectUnselected().width();
    }

    qreal height() const {
        return boundingRectUnselected().height();
    }

    QPointF center() const {
        return boundingRectUnselected().center();
    }

    QPointF centerSceneCoords() const {
        return mapToScene(center());
    }
};
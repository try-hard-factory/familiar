#include "borderdot.h"

#include <QBrush>
#include <QColor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <core/settingshandler.h>

DotSignal::DotSignal(QGraphicsItem* parentItem, QObject* parent)
    : QObject(parent)
{
    setZValue(999999999);
    //    setFlags(ItemIsMovable);
    setParentItem(parentItem);
    setAcceptHoverEvents(true);

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &DotSignal::settingsChangedSlot);

    settingsChangedSlot();

    setBrush(QBrush(selectionColor_));
    QPen outline_pen{selectionColor_, 0};
    setPen(outline_pen);
    int x = 4;
    setRect(-x, -x, 2 * x, 2 * x);
    setDotFlags(0);
    //    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

// DotSignal::DotSignal(QPointF pos, QGraphicsItem* parentItem, QObject* parent)
//     : QObject(parent)
// {
//     setZValue(999999999);
//     //    setFlags(ItemIsMovable);
//     setParentItem(parentItem);
//     setAcceptHoverEvents(true);
//     setBrush(QBrush(QColor(22, 142, 153)));
//     QPen outline_pen{QColor(22, 142, 153), 0};
//     setPen(outline_pen);
//     int x = 4;
//     setRect(-x, -x, 2 * x, 2 * x);
//     setPos(pos);
//     setPreviousPosition(pos);
//     setDotFlags(0);
//     //    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
// }

DotSignal::~DotSignal() {}

void DotSignal::SetScale(qreal qrScale)
{
    resetTransform();
    setScale(qrScale);
}

void DotSignal::settingsChangedSlot()
{
    auto settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    selectionColor_ = colorPreset[EPresetsColorIdx::kSelectionColor];
    setBrush(QBrush(selectionColor_));
    QPen outline_pen{selectionColor_, 0};
    setPen(outline_pen);
}


QPointF DotSignal::previousPosition() const noexcept
{
    return m_previousPosition;
}

void DotSignal::setPreviousPosition(const QPointF previousPosition) noexcept
{
    if (m_previousPosition == previousPosition)
        return;

    m_previousPosition = previousPosition;
    emit previousPositionChanged();
}

void DotSignal::setDotFlags(unsigned int flags)
{
    m_flags = flags;
}

void DotSignal::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_flags & Movable) {
        qDebug() << "DotSignal::mouseMoveEvent";
        auto dx = event->scenePos().x() - m_previousPosition.x();
        auto dy = event->scenePos().y() - m_previousPosition.y();
        moveBy(dx, dy);
        setPreviousPosition(event->scenePos());
        //        emit signalMove(this, dx, dy);
    } else {
        qDebug() << "else DotSignal::mouseMoveEvent";
        QGraphicsItem::mouseMoveEvent(event);
    }
}

void DotSignal::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_flags & Movable) {
        setPreviousPosition(event->scenePos());
    } else {
        QGraphicsItem::mousePressEvent(event);
    }
}

void DotSignal::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit signalMouseRelease();
    QGraphicsItem::mouseReleaseEvent(event);
}

void DotSignal::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    setBrush(QBrush(Qt::red));
}

void DotSignal::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    setBrush(QBrush(Qt::black));
}

#include "borderdot.h"
#include <QPainter>
BorderDot::BorderDot(QGraphicsItem * parent) :
    QGraphicsItem(parent)
{
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsGeometryChanges);
}


void BorderDot::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setBrush(Qt::blue);
    painter->setPen(Qt::darkGray);
    painter->drawEllipse(-5,-5,10,10);
}

QVariant BorderDot::itemChange(GraphicsItemChange change, const QVariant & value)
{
    switch(change) {
        case QGraphicsItem::ItemSelectedHasChanged:
            qWarning() << "vertex: " + value.toString(); // never happened
            break;
        default:
            break;
    }
    return QGraphicsItem::itemChange(change, value);
}

#ifndef BORDERDOT_H
#define BORDERDOT_H

#include <QGraphicsItem>

class BorderDot : public QGraphicsItem
{
public:
    BorderDot(QGraphicsItem * parent);
protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant & value) override;

};

#endif // BORDERDOT_H

#pragma once
#include <QRectF>

template<typename M, typename S>
void centered_widget(M* mw, S* w)
{
    auto mwPos = mw->mapToGlobal(QPoint(0, 0));
    auto mwSize = mw->size();

    auto wSize = w->size();
    auto point = mwPos + QPoint((mwSize.width() - wSize.width()) / 2, (mwSize.height() - wSize.height()) / 2);

    w->move(point);
    // qDebug() << hostRect;
    // qDebug() << point;
    // w->move(point);e
}

QRectF get_rect_from_points(const QPointF& point1, const QPointF& point2);

double roundTo(double number, double base);
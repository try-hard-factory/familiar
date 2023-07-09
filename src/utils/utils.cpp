#include "utils.h"

QRectF get_rect_from_points(const QPointF& point1, const QPointF& point2) {
    QPointF topLeft(std::min(point1.x(), point2.x()), std::min(point1.y(), point2.y()));
    QPointF bottomRight(std::max(point1.x(), point2.x()), std::max(point1.y(), point2.y()));
    return QRectF(topLeft, bottomRight);
}

double roundTo(double number, double base) {
    return base * std::round(number / base);
}
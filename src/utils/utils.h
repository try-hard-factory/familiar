#pragma once
#include <QRectF>
#include <QString>

template<typename M, typename S>
void centered_widget(M* mw, S* w)
{
    auto mwPos = mw->mapToGlobal(QPoint(0, 0));
    auto mwSize = mw->size();

    auto wSize = w->size();
    auto point = mwPos
                 + QPoint((mwSize.width() - wSize.width()) / 2,
                          (mwSize.height() - wSize.height()) / 2);

    w->move(point);
}

QRectF get_rect_from_points(const QPointF& point1, const QPointF& point2);

double roundTo(double number, double base);

// Base directory a portable (RUN_IN_PLACE) build keeps settings.json/
// recovery/the log file under - right next to the executable, instead
// of the OS's per-user standard-paths location (AppData\Local/.config/
// Application Support). Empty on a non-portable build - callers fall
// back to their usual QStandardPaths lookup in that case, unchanged.
QString portableDataDir();
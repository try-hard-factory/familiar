#include "utils.h"

#ifdef RUN_IN_PLACE
#include <QCoreApplication>
#include <QDir>
#endif

QRectF get_rect_from_points(const QPointF& point1, const QPointF& point2)
{
    QPointF topLeft(std::min(point1.x(), point2.x()),
                    std::min(point1.y(), point2.y()));
    QPointF bottomRight(std::max(point1.x(), point2.x()),
                        std::max(point1.y(), point2.y()));
    return QRectF(topLeft, bottomRight);
}

double roundTo(double number, double base)
{
    return base * std::round(number / base);
}

QString portableDataDir()
{
#ifdef RUN_IN_PLACE
    // A "data" sibling folder, not directly next to the .exe - keeps
    // settings.json/recovery/the log file from cluttering the same
    // directory a user might unzip the portable build into alongside
    // other files.
    return QDir(qApp->applicationDirPath()).filePath(QStringLiteral("data"));
#else
    return QString();
#endif
}
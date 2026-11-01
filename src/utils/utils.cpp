#include "utils.h"
#include <algorithm>
#include <cmath>

#include <QIODevice>
#include <QPoint>
#include <QSaveFile>
#include <QString>

#include "log/log.h"
using namespace familiar::log;

#ifdef RUN_IN_PLACE
#include <QCoreApplication>
#include <QDir>
#endif

QRectF get_rect_from_points(const QPointF& point1, const QPointF& point2)
{
    const QPointF topLeft(std::min(point1.x(), point2.x()),
                          std::min(point1.y(), point2.y()));
    const QPointF bottomRight(std::max(point1.x(), point2.x()),
                              std::max(point1.y(), point2.y()));
    return {topLeft, bottomRight};
}

double round_to(double number, double base)
{
    return base * std::round(number / base);
}

QString portable_data_dir()
{
#ifdef RUN_IN_PLACE
    // A "data" sibling folder, not directly next to the .exe - keeps
    // settings.json/recovery/the log file from cluttering the same
    // directory a user might unzip the portable build into alongside
    // other files.
    return QDir(qApp->applicationDirPath()).filePath(QStringLiteral("data"));
#else
    return {};
#endif
}

bool write_file_atomically(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    // No QIODevice::Truncate: a QSaveFile always starts out empty, and
    // the real file is not touched at all until commit() succeeds.
    if (!file.open(QIODevice::WriteOnly)) {
        FLOG_WARN(Ch::IO,
                  "Could not open {} for writing: {}",
                  path,
                  file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        FLOG_WARN(Ch::IO,
                  "Could not write all {} bytes to {}: {}",
                  bytes.size(),
                  path,
                  file.errorString());
        // The destructor already discards the temporary file when
        // commit() was never called, so the target is safe either way -
        // this states the abort explicitly, so that a commit() added
        // below this point later can't quietly publish a short write.
        file.cancelWriting();
        return false;
    }
    // Does the flush, the fsync and the rename; false if any of them -
    // or any earlier write() - failed.
    if (!file.commit()) {
        FLOG_WARN(Ch::IO,
                  "Could not commit {}: {}",
                  path,
                  file.errorString());
        return false;
    }
    return true;
}

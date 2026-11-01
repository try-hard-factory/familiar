#pragma once
#include <QByteArray>
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

double round_to(double number, double base);

// Base directory a portable (RUN_IN_PLACE) build keeps settings.json/
// recovery/the log file under - right next to the executable, instead
// of the OS's per-user standard-paths location (AppData\Local/.config/
// Application Support). Empty on a non-portable build - callers fall
// back to their usual QStandardPaths lookup in that case, unchanged.
QString portable_data_dir();

// Replaces `path`'s contents with `bytes` without ever leaving a
// truncated or empty file behind. QSaveFile writes a temporary file,
// fsyncs it and only then replaces the target with an atomic rename, so
// an interrupted write loses the new contents rather than the old ones.
//
// Plain QFile with QIODevice::Truncate cannot give that: it destroys the
// old contents at open() time, before a single byte is written, and
// never syncs - so a power loss any time within the kernel's writeback
// delay (vm.dirty_expire_centisecs, 30s by default) finds the file on
// disk empty even though the process wrote it successfully.
//
// Unlike QFile, nothing fails quietly: a short write (full disk/quota),
// a flush error and a failed rename all come back as false, with the
// reason logged on Ch::IO. The short write is the one worth noting -
// QFile buffers, so write() still reports every byte accepted and only
// the flush inside commit() finds out.
//
// One thing this gives up, inherently: the temporary file is created in
// the target's own directory, so a writable file inside a directory
// that is NOT writable can no longer be updated, where a plain QFile
// could have overwritten it in place.
bool write_file_atomically(const QString& path, const QByteArray& bytes);
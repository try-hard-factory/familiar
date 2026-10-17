#include "recovery.h"

#include "canvasscene.h"
#include "canvasview.h"
#include "fml_archive.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "log/log.h"
#include "utils/utils.h"
using namespace familiar::log;

namespace familiar::recovery {
namespace {

QString id_stem(const QUuid& id)
{
    return id.toString(QUuid::WithoutBraces);
}

QString sidecar_path_for(const QDir& dir, const QUuid& id)
{
    return dir.filePath(id_stem(id) + QStringLiteral(".json"));
}

QString fml_path_for(const QDir& dir, const QUuid& id)
{
    return dir.filePath(id_stem(id) + QStringLiteral(".fml"));
}

} // namespace

QString recovery_dir()
{
    QString dir = portable_data_dir();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
    }
    return dir + QStringLiteral("/recovery");
}

void save(CanvasView* canvasView)
{
    const QDir dir(recovery_dir());
    QDir().mkpath(dir.absolutePath());

    const QUuid id = canvasView->recovery_id();
    const QString fmlPath = fml_path_for(dir, id);

    const FmlResult result = FmlArchive::save(canvasView->scene(),
                                              canvasView->canvas_rect(),
                                              fmlPath);
    if (!result.error.isEmpty()) {
        FLOG_WARN(Ch::IO,
                  "Could not write recovery snapshot for {}: {}",
                  canvasView->path().toStdString(),
                  result.error.toStdString());
        return;
    }

    const bool untitled = canvasView->is_untitled();
    QJsonObject sidecar;
    sidecar[QStringLiteral("originalPath")] = untitled ? QString()
                                                       : canvasView->path();
    sidecar[QStringLiteral("label")]
        = untitled ? QStringLiteral("Untitled (%1)")
                         .arg(QDateTime::currentDateTime().toString(
                             QStringLiteral("dd.MM HH:mm")))
                   : QFileInfo(canvasView->path()).fileName();

    QFile sidecarFile(sidecar_path_for(dir, id));
    if (sidecarFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        sidecarFile.write(QJsonDocument(sidecar).toJson(QJsonDocument::Compact));
    } else {
        FLOG_WARN(Ch::IO,
                  "Could not write recovery sidecar for {}",
                  canvasView->path().toStdString());
    }
}

void remove(const QUuid& id)
{
    const QDir dir(recovery_dir());
    QFile::remove(fml_path_for(dir, id));
    QFile::remove(sidecar_path_for(dir, id));
}

void clear()
{
    QDir dir(recovery_dir());
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

QList<Entry> scan()
{
    QList<Entry> entries;
    const QDir dir(recovery_dir());
    if (!dir.exists()) {
        return entries;
    }

    const QStringList fmlFiles = dir.entryList({QStringLiteral("*.fml")},
                                               QDir::Files);
    for (const QString& fmlFile : fmlFiles) {
        const QUuid id = QUuid::fromString(
            QFileInfo(fmlFile).completeBaseName());
        if (id.isNull()) {
            continue;
        }

        Entry entry;
        entry.id = id;
        entry.fmlPath = dir.filePath(fmlFile);

        QFile sidecarFile(sidecar_path_for(dir, id));
        if (sidecarFile.open(QIODevice::ReadOnly)) {
            const QJsonObject obj
                = QJsonDocument::fromJson(sidecarFile.readAll()).object();
            entry.originalPath
                = obj.value(QStringLiteral("originalPath")).toString();
            entry.label = obj.value(QStringLiteral("label")).toString();
        }
        if (entry.label.isEmpty()) {
            entry.label = entry.originalPath.isEmpty()
                              ? QStringLiteral("Untitled")
                              : QFileInfo(entry.originalPath).fileName();
        }

        entries.append(entry);
    }
    return entries;
}

} // namespace familiar::recovery

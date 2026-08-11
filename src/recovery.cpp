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
using namespace familiar::log;

namespace familiar::recovery {
namespace {

QString idStem(const QUuid& id)
{
    return id.toString(QUuid::WithoutBraces);
}

QString sidecarPathFor(const QDir& dir, const QUuid& id)
{
    return dir.filePath(idStem(id) + QStringLiteral(".json"));
}

QString fmlPathFor(const QDir& dir, const QUuid& id)
{
    return dir.filePath(idStem(id) + QStringLiteral(".fml"));
}

} // namespace

QString recoveryDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/recovery");
}

void save(CanvasView* canvasView)
{
    const QDir dir(recoveryDir());
    QDir().mkpath(dir.absolutePath());

    const QUuid id = canvasView->recoveryId();
    const QString fmlPath = fmlPathFor(dir, id);

    FmlResult result = FmlArchive::save(canvasView->scene(),
                                        canvasView->canvasRect(),
                                        fmlPath);
    if (!result.error.isEmpty()) {
        FLOG_WARN(Ch::IO,
                  "Could not write recovery snapshot for {}: {}",
                  canvasView->path().toStdString(),
                  result.error.toStdString());
        return;
    }

    const bool untitled = canvasView->isUntitled();
    QJsonObject sidecar;
    sidecar[QStringLiteral("originalPath")] = untitled ? QString()
                                                       : canvasView->path();
    sidecar[QStringLiteral("label")]
        = untitled ? QStringLiteral("Untitled (%1)")
                         .arg(QDateTime::currentDateTime().toString(
                             QStringLiteral("dd.MM HH:mm")))
                   : QFileInfo(canvasView->path()).fileName();

    QFile sidecarFile(sidecarPathFor(dir, id));
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
    const QDir dir(recoveryDir());
    QFile::remove(fmlPathFor(dir, id));
    QFile::remove(sidecarPathFor(dir, id));
}

void clear()
{
    QDir dir(recoveryDir());
    if (dir.exists())
        dir.removeRecursively();
}

QList<Entry> scan()
{
    QList<Entry> entries;
    const QDir dir(recoveryDir());
    if (!dir.exists())
        return entries;

    const QStringList fmlFiles = dir.entryList({QStringLiteral("*.fml")},
                                               QDir::Files);
    for (const QString& fmlFile : fmlFiles) {
        const QUuid id = QUuid::fromString(
            QFileInfo(fmlFile).completeBaseName());
        if (id.isNull())
            continue;

        Entry entry;
        entry.id = id;
        entry.fmlPath = dir.filePath(fmlFile);

        QFile sidecarFile(sidecarPathFor(dir, id));
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

#include "pureref_import.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QTransform>
#include <QUuid>
#include <QtEndian>

#include <cmath>
#include <numbers>
#include <optional>

namespace familiar::pureref {
namespace {

// PureRef 2.x stores a 108-byte wrapper before its thumbnail and rotated SQLite data.
constexpr qint64 kHeaderSize = 108;
constexpr qint64 kMaxProjectSize = 1024LL * 1024 * 1024;
constexpr qint64 kCopyChunkSize = 1024 * 1024;

struct SourceItem {
    qint64 id = -1;
    qint64 parent = -1;
    qreal z = 0;
    qreal opacity = 1;
    QString name;
    QTransform transform;
    QByteArray imageBytes;
    QString imageFormat;
    QString imageSource;
    QTransform imageTransform;
    QByteArray boundsBytes;
    QString noteHtml;
    QString noteColor;
    QString groupColor;
    bool image = false;
    bool note = false;
    bool group = false;
    bool drawing = false;
    bool locked = false;
};

QByteArray unwrapped_bytes(const QVariant& value)
{
    // PureRef stores Qt's binary streams as Latin-1 characters in SQLite
    // TEXT columns. CAST(... AS BLOB) gives their UTF-8 encoding back.
    return QString::fromUtf8(value.toByteArray()).toLatin1();
}

std::optional<QTransform> read_transform(const QVariant& value)
{
    constexpr qsizetype kPrefixSize = 5;
    constexpr qsizetype kMatrixValues = 9; // QTransform is a 3x3 matrix.
    constexpr qsizetype kEncodedSize = kPrefixSize + kMatrixValues * sizeof(double);
    const QByteArray bytes = unwrapped_bytes(value);
    // Five-byte Qt stream prefix, followed by nine big-endian doubles.
    if (bytes.size() != kEncodedSize
        || bytes.left(kPrefixSize) != QByteArray::fromHex("0000005000")) {
        return std::nullopt;
    }
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly) || !buffer.seek(kPrefixSize)) {
        return std::nullopt;
    }
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    double m[kMatrixValues]{};
    for (double& v : m) {
        stream >> v;
        if (!std::isfinite(v)) {
            return std::nullopt;
        }
    }
    if (stream.status() != QDataStream::Ok) {
        return std::nullopt;
    }
    return QTransform(m[0], m[1], m[2], m[3], m[4], m[5],
                      m[6], m[7], m[8]);
}

std::optional<QRectF> read_image_bounds(const QByteArray& bytes,
                                         const QTransform& imageTransform)
{
    // Bounds use a serialized QPainterPath. The 34-byte check is an early
    // guard; the full point count and payload length are checked below.
    if (bytes.size() < 34 || bytes.left(8) != QByteArray::fromHex("0000040000000000")) {
        return std::nullopt;
    }
    const int tagLength = static_cast<quint8>(bytes[8]);
    if (tagLength != 13 || bytes.mid(9, tagLength) != QByteArray("QPainterPath\0", 13)) {
        return std::nullopt;
    }
    const int countOffset = 9 + tagLength;
    const quint32 count = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar*>(bytes.constData() + countOffset));
    // A rectangle needs at least four points; each path point is a 4-byte type
    // and two 8-byte coordinates. Cap the count before allocating or reading.
    if (count < 4 || count > 100000
        || bytes.size() < countOffset + 4 + static_cast<qint64>(count) * 20) {
        return std::nullopt;
    }
    bool invertible = false;
    const QTransform inverse = imageTransform.inverted(&invertible);
    if (!invertible) {
        return std::nullopt;
    }
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly) || !buffer.seek(countOffset + 4)) {
        return std::nullopt;
    }
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    QRectF bounds;
    for (quint32 i = 0; i < count; ++i) {
        quint32 type = 0;
        double x = 0;
        double y = 0;
        stream >> type >> x >> y;
        if (stream.status() != QDataStream::Ok || !std::isfinite(x)
            || !std::isfinite(y)) {
            return std::nullopt;
        }
        const QPointF p = inverse.map(QPointF(x, y));
        if (i == 0) {
            bounds = QRectF(p, p);
        } else {
            bounds = bounds.united(QRectF(p, p));
        }
    }
    return bounds.normalized();
}

bool copy_range(QFile& source, QTemporaryFile& target, qint64 from,
                qint64 length, const std::function<bool()>& isCanceled)
{
    if (!source.seek(from)) {
        return false;
    }
    while (length > 0) {
        if (isCanceled && isCanceled()) {
            return false;
        }
        const QByteArray chunk = source.read(qMin(length, kCopyChunkSize));
        if (chunk.isEmpty() || target.write(chunk) != chunk.size()) {
            return false;
        }
        length -= chunk.size();
    }
    return true;
}

QTransform scene_transform(qint64 id, const QMap<qint64, SourceItem>& source,
                           QStringList& warnings)
{
    QTransform result;
    QSet<qint64> visited;
    // The depth cap also protects against an implausibly deep parent chain;
    // visited catches cycles before we reach it.
    for (int depth = 0; depth < 100 && id != -1; ++depth) {
        if (visited.contains(id) || !source.contains(id)) {
            warnings << QObject::tr("A PureRef group has an invalid parent chain.");
            break;
        }
        visited.insert(id);
        const auto it = source.constFind(id);
        const SourceItem& item = it.value();
        result *= item.transform;
        id = item.parent;
    }
    return result;
}

void append_item(const SourceItem& sourceItem,
                 const QMap<qint64, SourceItem>& source,
                 const QMap<qint64, QUuid>& ids,
                 ImportResult& result)
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), ids[sourceItem.id]);
    item.insert(QStringLiteral("z"), sourceItem.z);

    QTransform transform = scene_transform(sourceItem.id, source, result.warnings);
    if (sourceItem.image) {
        transform = sourceItem.imageTransform * transform;
    }
    const QPointF origin = transform.map(QPointF(0, 0));
    const qreal scale = std::hypot(transform.m11(), transform.m12());
    const qreal otherScale = std::hypot(transform.m21(), transform.m22());
    if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(origin.x())
        || !std::isfinite(origin.y())) {
        result.warnings << QObject::tr("Skipped item %1: invalid transform.")
                               .arg(sourceItem.id);
        return;
    }
    item.insert(QStringLiteral("x"), origin.x());
    item.insert(QStringLiteral("y"), origin.y());
    item.insert(QStringLiteral("scale"), scale);
    item.insert(QStringLiteral("rotation"),
                std::atan2(transform.m12(), transform.m11()) * 180.0
                    / std::numbers::pi);
    // 1e-9 catches projective components; 1e-4 tolerates rounding in scale
    // and orthogonality before reporting a transform we cannot reproduce.
    if (std::abs(transform.m13()) > 1e-9 || std::abs(transform.m23()) > 1e-9
        || std::abs(transform.m33() - 1) > 1e-9
        || std::abs(otherScale - scale) > 1e-4
        || transform.determinant() < 0
        || std::abs(transform.m11() * transform.m21()
                    + transform.m12() * transform.m22()) > 1e-4) {
        result.warnings << QObject::tr("Item %1 uses a transform Familiar cannot reproduce exactly.")
                               .arg(sourceItem.id);
    }

    QVariantMap extra;
    if (sourceItem.image) {
        QBuffer imageBuffer;
        imageBuffer.setData(sourceItem.imageBytes);
        imageBuffer.open(QIODevice::ReadOnly);
        QImageReader reader(&imageBuffer);
        reader.setAutoTransform(true);
        const bool exifTransformed = reader.transformation()
                                     != QImageIOHandler::TransformationNone;
        const QImage image = reader.read();
        if (image.isNull()) {
            result.warnings << QObject::tr("Skipped image %1: unsupported or damaged image data.")
                                   .arg(sourceItem.name);
            return;
        }
        const QString format = sourceItem.imageFormat.toLower();
        const bool animated = format == QLatin1String("gif");
        item.insert(QStringLiteral("type"), animated ? QStringLiteral("gif")
                                                    : QStringLiteral("pixmap"));
        if (animated) {
            item.insert(QStringLiteral("gifBytes"), sourceItem.imageBytes);
        } else {
            item.insert(QStringLiteral("image"), QVariant::fromValue(image));
            // Prepare a 1/8-size mip source for large images off the GUI thread.
            if (image.width() > 512 || image.height() > 512) {
                item.insert(QStringLiteral("renderPreview"),
                            QVariant::fromValue(image.scaled(
                                image.size() / 8,
                                Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation)));
            }
            if (!exifTransformed) {
                item.insert(QStringLiteral("sourceBytes"), sourceItem.imageBytes);
                item.insert(QStringLiteral("sourceFormat"), format);
            }
        }
        QString filename = QFileInfo(sourceItem.imageSource).fileName();
        if (filename.isEmpty()) {
            filename = sourceItem.name + QStringLiteral(".") + format;
        }
        item.insert(QStringLiteral("filename"), filename);
        extra.insert(QStringLiteral("opacity"), sourceItem.opacity);
        const auto crop = read_image_bounds(sourceItem.boundsBytes,
                                            sourceItem.imageTransform);
        if (crop && !crop->isEmpty()) {
            const QRectF full(image.rect());
            const QRectF visible = crop->intersected(full);
            if (!visible.isEmpty()) {
                extra.insert(QStringLiteral("crop"),
                             QVariantList{visible.x(), visible.y(),
                                          visible.width(), visible.height()});
                result.bounds = result.bounds.united(transform.mapRect(visible));
            } else {
                result.bounds = result.bounds.united(transform.mapRect(full));
            }
        } else {
            result.bounds = result.bounds.united(transform.mapRect(QRectF(image.rect())));
        }
    } else if (sourceItem.note) {
        item.insert(QStringLiteral("type"), QStringLiteral("text"));
        extra.insert(QStringLiteral("html"), sourceItem.noteHtml);
        extra.insert(QStringLiteral("fill_color"), sourceItem.noteColor);
        // PureRef does not give us a simple note rectangle here; use an
        // approximate size only for the imported board's initial bounds.
        result.bounds = result.bounds.united(QRectF(origin, QSizeF(200 * scale, 100 * scale)));
    } else if (sourceItem.group) {
        item.insert(QStringLiteral("type"), QStringLiteral("group"));
        QVariantList children;
        for (auto child = source.cbegin(); child != source.cend(); ++child) {
            if (child->parent == sourceItem.id && ids.contains(child.key())) {
                children << ids[child.key()].toString(QUuid::WithoutBraces);
            }
        }
        extra.insert(QStringLiteral("child_ids"), children);
        if (!sourceItem.groupColor.isEmpty()) {
            extra.insert(QStringLiteral("fill_color"), sourceItem.groupColor);
        }
        if (sourceItem.locked) {
            extra.insert(QStringLiteral("locked"), true);
        }
        // Familiar groups are scene-space containers. Their source
        // transform has already been folded into each descendant.
        item.insert(QStringLiteral("x"), 0.0);
        item.insert(QStringLiteral("y"), 0.0);
        item.insert(QStringLiteral("scale"), 1.0);
        item.insert(QStringLiteral("rotation"), 0.0);
    } else {
        return;
    }
    if (sourceItem.locked && !sourceItem.group) {
        result.warnings << QObject::tr("PureRef lock on item %1 was not imported.")
                               .arg(sourceItem.id);
    }
    if (!sourceItem.image && std::abs(sourceItem.opacity - 1.0) > 1e-5) {
        result.warnings << QObject::tr("PureRef opacity on item %1 was not imported.")
                               .arg(sourceItem.id);
    }
    item.insert(QStringLiteral("data"), extra);
    result.items << item;
}

ImportResult read_database(QSqlDatabase& db,
                           const std::function<bool()>& isCanceled)
{
    ImportResult result;
    QSqlQuery integrity(db);
    if (!integrity.exec(QStringLiteral("PRAGMA integrity_check"))
        || !integrity.next() || integrity.value(0).toString() != QLatin1String("ok")) {
        result.error = QObject::tr("The PureRef project database is damaged.");
        return result;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT i.id,i.parent,i.z,i.opacity,i.locked,i.name,CAST(i.transform AS BLOB),"
            "im.data,im.format,im.source,CAST(ii.image_transform AS BLOB),"
            "CAST(ii.image_bounds AS BLOB),n.text,n.background_color,g.background_color,"
            "g.id,d.id FROM items i "
            "LEFT JOIN items_images ii ON ii.id=i.id "
            "LEFT JOIN images im ON im.id=ii.image "
            "LEFT JOIN items_notes n ON n.id=i.id "
            "LEFT JOIN items_groups g ON g.id=i.id "
            "LEFT JOIN items_drawings d ON d.id=i.id ORDER BY i.z,i.id"))) {
        result.error = QObject::tr("Unsupported PureRef project schema: %1")
                           .arg(query.lastError().text());
        return result;
    }
    QMap<qint64, SourceItem> source;
    QList<qint64> order;
    while (query.next()) {
        if (isCanceled && isCanceled()) {
            result.error = QObject::tr("PureRef import canceled.");
            return result;
        }
        SourceItem item;
        item.id = query.value(0).toLongLong();
        item.parent = query.value(1).toLongLong();
        item.z = query.value(2).toDouble();
        item.opacity = query.value(3).toDouble();
        item.locked = query.value(4).toBool();
        item.name = query.value(5).toString();
        const auto transform = read_transform(query.value(6));
        if (!transform) {
            result.warnings << QObject::tr("Skipped item %1: invalid transform.")
                                   .arg(item.id);
            continue;
        }
        item.transform = *transform;
        item.image = !query.value(7).isNull();
        item.note = !query.value(12).isNull();
        item.group = !query.value(15).isNull();
        item.drawing = !query.value(16).isNull();
        if (item.image) {
            item.imageBytes = query.value(7).toByteArray();
            item.imageFormat = query.value(8).toString();
            item.imageSource = query.value(9).toString();
            const auto imageTransform = read_transform(query.value(10));
            if (!imageTransform) {
                result.warnings << QObject::tr("Skipped image %1: invalid image transform.")
                                       .arg(item.name);
                continue;
            }
            item.imageTransform = *imageTransform;
            item.boundsBytes = unwrapped_bytes(query.value(11));
        } else if (item.note) {
            item.noteHtml = query.value(12).toString();
            item.noteColor = query.value(13).toString();
        } else if (item.group) {
            item.groupColor = query.value(14).toString();
        } else if (item.drawing) {
            result.warnings << QObject::tr("PureRef drawing %1 cannot be imported yet.")
                                   .arg(item.id);
            continue;
        } else {
            result.warnings << QObject::tr("Skipped unknown PureRef item %1.")
                                   .arg(item.id);
            continue;
        }
        order << item.id;
        source.insert(item.id, std::move(item));
    }
    QMap<qint64, QUuid> ids;
    for (qint64 id : order) {
        ids.insert(id, QUuid::createUuid());
    }
    for (qint64 id : order) {
        if (isCanceled && isCanceled()) {
            result.items.clear();
            result.error = QObject::tr("PureRef import canceled.");
            return result;
        }
        const auto it = source.constFind(id);
        append_item(it.value(), source, ids, result);
    }
    return result;
}

} // namespace

ImportResult read_project(const QString& path,
                          const std::function<bool()>& isCanceled)
{
    ImportResult result;
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) {
        result.error = QObject::tr("Could not read %1: %2")
                           .arg(path, source.errorString());
        return result;
    }
    const qint64 fileSize = source.size();
    if (fileSize < kHeaderSize || fileSize > kMaxProjectSize) {
        result.error = QObject::tr("The PureRef project is empty or exceeds the 1 GB import limit.");
        return result;
    }
    const QByteArray header = source.read(kHeaderSize);
    // The first field is PureRef's file marker; the next eight bytes encode
    // "2.1" as UTF-16BE with a terminating NUL.
    if (header.size() != kHeaderSize
        || qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(header.constData())) != 6
        || header.mid(4, 8) != QByteArray::fromHex("0032002e00310000")) {
        result.error = QObject::tr("Only PureRef 2.x .pur projects are supported.");
        return result;
    }
    const auto* raw = reinterpret_cast<const uchar*>(header.constData());
    // Byte 14 stores the SQLite length; byte 104 stores the thumbnail length.
    // Together with the fixed header, the latter locates the database payload.
    const quint64 databaseSize = qFromBigEndian<quint64>(raw + 14);
    const quint32 thumbnailSize = qFromBigEndian<quint32>(raw + 104);
    const qint64 split = kHeaderSize + thumbnailSize;
    if (split >= fileSize || databaseSize < static_cast<quint64>(split)
        || databaseSize != static_cast<quint64>(fileSize - split)) {
        result.error = QObject::tr("The PureRef project has an invalid database length.");
        return result;
    }
    QByteArray expectedDigest;
    // Bytes 40..103 hold the MD5 digest as UTF-16BE hex characters. The hash
    // itself covers the file from byte 104 onward, including thumbnail length.
    expectedDigest.reserve(32);
    for (int i = 40; i < 104; i += 2) {
        if (header[i] != 0) {
            result.error = QObject::tr("The PureRef project header is damaged.");
            return result;
        }
        expectedDigest.append(header[i + 1]);
    }
    if (!source.seek(104)) {
        result.error = QObject::tr("Could not read the PureRef project.");
        return result;
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!source.atEnd()) {
        if (isCanceled && isCanceled()) {
            result.error = QObject::tr("PureRef import canceled.");
            return result;
        }
        const QByteArray chunk = source.read(kCopyChunkSize);
        if (chunk.isEmpty()) {
            result.error = QObject::tr("Could not read the complete PureRef project.");
            return result;
        }
        hash.addData(chunk);
    }
    if (hash.result().toHex() != expectedDigest) {
        result.error = QObject::tr("The PureRef project checksum does not match.");
        return result;
    }

    QTemporaryFile databaseFile;
    const qint64 dbLength = static_cast<qint64>(databaseSize);
    // The SQLite bytes are rotated by `split`: their first `split` bytes are
    // stored at file offset `dbLength`, followed by the remaining bytes.
    if (!databaseFile.open()
        || !copy_range(source, databaseFile, dbLength, split, isCanceled)
        || !copy_range(source, databaseFile, split, dbLength - split,
                       isCanceled)
        || !databaseFile.flush()) {
        result.error = QObject::tr("Could not reconstruct the PureRef project database.");
        return result;
    }
    databaseFile.close();
    const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                     connectionName);
        db.setDatabaseName(databaseFile.fileName());
        if (!db.open()) {
            result.error = QObject::tr("Could not open the PureRef project database: %1")
                               .arg(db.lastError().text());
        } else {
            result = read_database(db, isCanceled);
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return result;
}

} // namespace familiar::pureref

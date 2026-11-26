#include "gltf_import.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QUrl>
#include <QVector>

namespace {
constexpr qint64 kmaxGlbBytes = 128LL * 1024 * 1024;

struct Resource
{
    QByteArray bytes;
    QString error;
};

QString image_mime_type(const QByteArray& bytes)
{
    if (bytes.startsWith("\x89PNG\r\n\x1a\n")) {
        return QStringLiteral("image/png");
    }
    if (bytes.startsWith("\xff\xd8\xff")) {
        return QStringLiteral("image/jpeg");
    }
    if (bytes.startsWith("RIFF") && bytes.mid(8, 4) == "WEBP") {
        return QStringLiteral("image/webp");
    }
    if (bytes.startsWith("\xabKTX 20\xbb\r\n\x1a\n")) {
        return QStringLiteral("image/ktx2");
    }
    return {};
}

Resource read_resource(const QString& uri, const QDir& sourceDir,
                       qint64 remaining)
{
    if (uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        const qsizetype comma = uri.indexOf(QLatin1Char(','));
        if (comma < 0) {
            return {{}, QObject::tr("Invalid data URI in model")};
        }
        const QString meta = uri.mid(5, comma - 5);
        const QString data = uri.mid(comma + 1);
        const bool base64 = meta.endsWith(QStringLiteral(";base64"),
                                           Qt::CaseInsensitive);
        const QByteArray bytes = base64
            ? QByteArray::fromBase64(data.toLatin1(),
                                     QByteArray::AbortOnBase64DecodingErrors)
            : QByteArray::fromPercentEncoding(data.toLatin1());
        if (bytes.isEmpty() || bytes.size() > remaining) {
            return {{}, QObject::tr("Model resource is empty or too large")};
        }
        return {bytes, {}};
    }

    const QUrl parsed(uri);
    const QString relative = QUrl::fromPercentEncoding(uri.toUtf8());
    if (!parsed.isRelative() || QFileInfo(relative).isAbsolute()) {
        return {{}, QObject::tr("Only local relative model resources are supported")};
    }
    const QString path = sourceDir.absoluteFilePath(relative);
    const QString canonicalRoot = QFileInfo(sourceDir.absolutePath()).canonicalFilePath();
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalPath.isEmpty()
        || !canonicalPath.startsWith(canonicalRoot + QDir::separator())) {
        return {{}, QObject::tr("Model resource is missing or outside its folder: %1")
                         .arg(relative)};
    }
    QFile file(canonicalPath);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > remaining) {
        return {{}, QObject::tr("Could not read model resource: %1")
                         .arg(relative)};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() != file.size()) {
        return {{}, QObject::tr("Could not read model resource: %1")
                         .arg(relative)};
    }
    return {bytes, {}};
}

bool append_aligned(QByteArray& target, const QByteArray& bytes)
{
    const qint64 padded = (bytes.size() + 3) & ~qint64(3);
    if (padded > kmaxGlbBytes - target.size()) {
        return false;
    }
    target.append(bytes);
    while (target.size() % 4) {
        target.append('\0');
    }
    return true;
}
} // namespace

GltfImportResult import_gltf(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > kmaxGlbBytes) {
        return {{}, QObject::tr("Could not read .gltf file (maximum 128 MB)")};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {{}, QObject::tr("Invalid .gltf JSON: %1")
                        .arg(parseError.errorString())};
    }
    QJsonObject json = document.object();
    if (json.value(QStringLiteral("asset")).toObject()
            .value(QStringLiteral("version")).toString() != QStringLiteral("2.0")) {
        return {{}, QObject::tr("Only glTF 2.0 models are supported")};
    }
    const QDir sourceDir(QFileInfo(filename).absolutePath());
    QByteArray binary;
    QJsonArray buffers = json.value(QStringLiteral("buffers")).toArray();
    QJsonArray views = json.value(QStringLiteral("bufferViews")).toArray();
    QVector<qint64> bufferOffsets;
    bufferOffsets.reserve(buffers.size());
    for (const QJsonValue& value : buffers) {
        const QJsonObject buffer = value.toObject();
        const qint64 declared = buffer.value(QStringLiteral("byteLength")).toInteger(-1);
        const QString uri = buffer.value(QStringLiteral("uri")).toString();
        if (declared < 0 || uri.isEmpty()) {
            return {{}, QObject::tr("A .gltf buffer has no valid URI or length")};
        }
        Resource resource = read_resource(uri, sourceDir,
                                          kmaxGlbBytes - binary.size());
        if (!resource.error.isEmpty()) {
            return {{}, resource.error};
        }
        if (resource.bytes.size() < declared) {
            return {{}, QObject::tr("A .gltf buffer is shorter than declared")};
        }
        bufferOffsets.append(binary.size());
        if (!append_aligned(binary, resource.bytes.left(declared))) {
            return {{}, QObject::tr("Model exceeds 128 MB after packing")};
        }
    }

    for (int i = 0; i < views.size(); ++i) {
        QJsonObject view = views.at(i).toObject();
        const int bufferIndex = view.value(QStringLiteral("buffer")).toInt(-1);
        const qint64 offset = view.value(QStringLiteral("byteOffset")).toInteger();
        const qint64 length = view.value(QStringLiteral("byteLength")).toInteger(-1);
        const qint64 declared = bufferIndex >= 0 && bufferIndex < buffers.size()
            ? buffers.at(bufferIndex).toObject()
                  .value(QStringLiteral("byteLength")).toInteger(-1)
            : -1;
        if (bufferIndex < 0 || bufferIndex >= bufferOffsets.size()
            || offset < 0 || length < 0 || declared < 0
            || offset > declared || length > declared - offset) {
            return {{}, QObject::tr("Invalid .gltf buffer view")};
        }
        view[QStringLiteral("buffer")] = 0;
        view[QStringLiteral("byteOffset")] = bufferOffsets.at(bufferIndex) + offset;
        QJsonObject extensions = view.value(QStringLiteral("extensions")).toObject();
        QJsonObject meshopt
            = extensions.value(QStringLiteral("EXT_meshopt_compression")).toObject();
        if (!meshopt.isEmpty()) {
            const int sourceBuffer = meshopt.value(QStringLiteral("buffer")).toInt(-1);
            const qint64 sourceOffset
                = meshopt.value(QStringLiteral("byteOffset")).toInteger();
            const qint64 sourceLength
                = meshopt.value(QStringLiteral("byteLength")).toInteger(-1);
            const qint64 sourceSize = sourceBuffer >= 0 && sourceBuffer < buffers.size()
                ? buffers.at(sourceBuffer).toObject()
                      .value(QStringLiteral("byteLength")).toInteger(-1)
                : -1;
            if (sourceBuffer < 0 || sourceBuffer >= bufferOffsets.size()
                || sourceOffset < 0 || sourceLength < 0 || sourceSize < 0
                || sourceOffset > sourceSize
                || sourceLength > sourceSize - sourceOffset) {
                return {{}, QObject::tr("Invalid compressed .gltf buffer view")};
            }
            meshopt[QStringLiteral("buffer")] = 0;
            meshopt[QStringLiteral("byteOffset")]
                = bufferOffsets.at(sourceBuffer) + sourceOffset;
            extensions[QStringLiteral("EXT_meshopt_compression")] = meshopt;
            view[QStringLiteral("extensions")] = extensions;
        }
        views.replace(i, view);
    }

    QJsonArray images = json.value(QStringLiteral("images")).toArray();
    for (int i = 0; i < images.size(); ++i) {
        QJsonObject image = images.at(i).toObject();
        const QString uri = image.value(QStringLiteral("uri")).toString();
        if (uri.isEmpty()) {
            continue; // Already stored in a bufferView.
        }
        Resource resource = read_resource(uri, sourceDir,
                                          kmaxGlbBytes - binary.size());
        if (!resource.error.isEmpty()) {
            return {{}, resource.error};
        }
        const QString mime = image_mime_type(resource.bytes);
        if (mime.isEmpty()) {
            return {{}, QObject::tr("Unsupported model image: %1").arg(uri)};
        }
        const qint64 offset = binary.size();
        const qint64 length = resource.bytes.size();
        if (!append_aligned(binary, resource.bytes)) {
            return {{}, QObject::tr("Model exceeds 128 MB after packing")};
        }
        QJsonObject view;
        view[QStringLiteral("buffer")] = 0;
        view[QStringLiteral("byteOffset")] = offset;
        view[QStringLiteral("byteLength")] = length;
        image.remove(QStringLiteral("uri"));
        image[QStringLiteral("bufferView")] = views.size();
        image[QStringLiteral("mimeType")] = mime;
        views.append(view);
        images.replace(i, image);
    }

    if (binary.isEmpty()) {
        return {{}, QObject::tr("The .gltf model contains no binary data")};
    }
    json[QStringLiteral("bufferViews")] = views;
    json[QStringLiteral("images")] = images;
    json[QStringLiteral("buffers")]
        = QJsonArray{QJsonObject{{QStringLiteral("byteLength"), binary.size()}}};
    QByteArray jsonBytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    while (jsonBytes.size() % 4) {
        jsonBytes.append(' ');
    }
    if (jsonBytes.size() + binary.size() + 28 > kmaxGlbBytes) {
        return {{}, QObject::tr("Model exceeds 128 MB after packing")};
    }
    QByteArray glb;
    QDataStream stream(&glb, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(0x46546C67) << quint32(2)
           << quint32(28 + jsonBytes.size() + binary.size())
           << quint32(jsonBytes.size()) << quint32(0x4E4F534A);
    stream.writeRawData(jsonBytes.constData(), jsonBytes.size());
    stream << quint32(binary.size()) << quint32(0x004E4942);
    stream.writeRawData(binary.constData(), binary.size());
    return {glb, {}};
}

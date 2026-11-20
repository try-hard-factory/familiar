#include "obj_import.h"

#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTextStream>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr qint64 kmaxGlbBytes = 128LL * 1024 * 1024;

struct VertexRef
{
    int position = -1;
    int uv = -1;
    int normal = -1;
    bool operator==(const VertexRef&) const = default;
};

size_t qHash(const VertexRef& ref, size_t seed = 0)
{
    return qHashMulti(seed, ref.position, ref.uv, ref.normal);
}

struct Vertex
{
    QVector3D position;
    QVector3D normal;
    QVector2D uv;
};

struct Primitive
{
    QVector<Vertex> vertices;
    QVector<quint32> indices;
    QHash<VertexRef, quint32> knownVertices;
    QVector3D minimum = QVector3D(std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::max());
    QVector3D maximum = -minimum;
};

struct Material
{
    QString name;
    QVector3D color = QVector3D(1, 1, 1);
    float opacity = 1;
    QString texture;
    QDir textureDir;
    QString basePath;
    QString normalPath;
    QString roughnessPath;
    QString metallicPath;
    QString occlusionPath;
    QString emissivePath;
};

QString asset_path(const QDir& root, const QDir& relativeTo,
                   QString relative)
{
    if (relative.size() >= 2 && relative.startsWith(QLatin1Char('"'))
        && relative.endsWith(QLatin1Char('"'))) {
        relative = relative.mid(1, relative.size() - 2);
    }
    relative.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (QFileInfo(relative).isAbsolute()) {
        return {};
    }
    const QString path = QFileInfo(relativeTo.absoluteFilePath(relative))
                             .canonicalFilePath();
    const QString rootPath = QFileInfo(root.absolutePath()).canonicalFilePath();
    if (path.isEmpty() || rootPath.isEmpty()
        || !path.startsWith(rootPath + QDir::separator())) {
        return {};
    }
    return path;
}

qsizetype command_end(const QString& line)
{
    for (qsizetype i = 0; i < line.size(); ++i) {
        if (line.at(i).isSpace()) {
            return i;
        }
    }
    return -1;
}

int ensure_material(const QString& name, QVector<Material>& materials,
                    QVector<Primitive>& primitives, QHash<QString, int>& names)
{
    const auto found = names.constFind(name);
    if (found != names.cend()) {
        return *found;
    }
    const int index = int(materials.size());
    Material material;
    material.name = name;
    materials.append(material);
    primitives.append(Primitive{});
    names.insert(name, index);
    return index;
}

QString map_path(QString value)
{
    // MTL map options precede the path. Leave spaces in the actual filename.
    QStringList parts = value.simplified().split(QLatin1Char(' '));
    int i = 0;
    while (i < parts.size() && parts.at(i).startsWith(QLatin1Char('-'))) {
        const QString option = parts.at(i++);
        const int arguments = option == QLatin1String("-s")
                                  || option == QLatin1String("-o")
                                  || option == QLatin1String("-t") ? 3
                            : option == QLatin1String("-mm") ? 2 : 1;
        i += arguments;
    }
    return parts.mid(i).join(QLatin1Char(' '));
}

QString normalized_name(const QString& text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar character : text) {
        if (character.isLetterOrNumber()) {
            result.append(character.toLower());
        }
    }
    return result;
}

void discover_textures(const QDir& modelDir, QVector<Material>& materials,
                       const QVector<Primitive>& primitives)
{
    QStringList folders{modelDir.absolutePath(),
                        modelDir.filePath(QStringLiteral("textures"))};
    QDir parent = modelDir;
    if (parent.cdUp()) {
        folders.append(parent.filePath(QStringLiteral("textures")));
    }
    QSet<QString> visited;
    for (const QString& folder : folders) {
        if (!QFileInfo(folder).isDir()) {
            continue;
        }
        QDirIterator files(folder, QDir::Files, QDirIterator::Subdirectories);
        int scanned = 0;
        while (files.hasNext() && scanned++ < 2000) {
            const QString path = QFileInfo(files.next()).canonicalFilePath();
            const QString canonicalFolder = QFileInfo(folder).canonicalFilePath();
            if (path.isEmpty() || canonicalFolder.isEmpty()
                || !path.startsWith(canonicalFolder + QDir::separator())) {
                continue;
            }
            if (visited.contains(path)) {
                continue;
            }
            visited.insert(path);
            const QString extension = QFileInfo(path).suffix().toLower();
            if (extension != QLatin1String("png")
                && extension != QLatin1String("jpg")
                && extension != QLatin1String("jpeg")
                && extension != QLatin1String("webp")
                && extension != QLatin1String("bmp")
                && extension != QLatin1String("tga")) {
                continue;
            }
            const QString stem = normalized_name(QFileInfo(path).completeBaseName());
            for (int i = 0; i < materials.size(); ++i) {
                Material& material = materials[i];
                if (material.name.isEmpty() || primitives[i].indices.isEmpty()) {
                    continue;
                }
                const QString name = normalized_name(material.name);
                const auto assign = [&](const QStringList& suffixes,
                                        QString& destination) {
                    if (!destination.isEmpty()) {
                        return;
                    }
                    for (const QString& suffix : suffixes) {
                        if (!stem.endsWith(suffix)) {
                            continue;
                        }
                        const QString prefix = stem.left(stem.size() - suffix.size());
                        if (prefix == name || prefix.endsWith(name)) {
                            destination = path;
                            return;
                        }
                    }
                };
                assign({QStringLiteral("basecolor"), QStringLiteral("albedo"),
                        QStringLiteral("diffuse"), QStringLiteral("color")},
                       material.basePath);
                assign({QStringLiteral("normalopengl"),
                        QStringLiteral("normalgl"), QStringLiteral("normal")},
                       material.normalPath);
                assign({QStringLiteral("roughness")}, material.roughnessPath);
                assign({QStringLiteral("metallic"), QStringLiteral("metalness")},
                       material.metallicPath);
                assign({QStringLiteral("ambientocclusion"),
                        QStringLiteral("mixedao"), QStringLiteral("occlusion"),
                        QStringLiteral("ao")}, material.occlusionPath);
                assign({QStringLiteral("emissive"), QStringLiteral("emission")},
                       material.emissivePath);
            }
        }
    }
}

QString parse_mtl(const QString& path,
                  QVector<Material>& materials, QVector<Primitive>& primitives,
                  QHash<QString, int>& names)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kmaxGlbBytes) {
        return QObject::tr("Could not read OBJ material file: %1").arg(path);
    }
    QTextStream input(&file);
    const QDir materialDir(QFileInfo(path).absolutePath());
    int current = -1;
    while (!input.atEnd()) {
        QString line = input.readLine();
        const qsizetype comment = line.indexOf(QLatin1Char('#'));
        if (comment >= 0) {
            line.truncate(comment);
        }
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const qsizetype space = command_end(line);
        if (space < 0) {
            continue;
        }
        const QString command = line.left(space);
        const QString value = line.mid(space + 1).trimmed();
        if (command == QLatin1String("newmtl")) {
            current = ensure_material(value, materials, primitives, names);
            materials[current].textureDir = materialDir;
        } else if (current >= 0 && command == QLatin1String("Kd")) {
            const QStringList values = value.simplified().split(QLatin1Char(' '));
            if (values.size() >= 3) {
                materials[current].color = QVector3D(
                    std::clamp(values[0].toFloat(), 0.0f, 1.0f),
                    std::clamp(values[1].toFloat(), 0.0f, 1.0f),
                    std::clamp(values[2].toFloat(), 0.0f, 1.0f));
            }
        } else if (current >= 0 && command == QLatin1String("d")) {
            materials[current].opacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
        } else if (current >= 0 && command == QLatin1String("Tr")) {
            materials[current].opacity = 1.0f - std::clamp(value.toFloat(), 0.0f, 1.0f);
        } else if (current >= 0 && command == QLatin1String("map_Kd")) {
            materials[current].texture = map_path(value);
        }
    }
    return {};
}

int obj_index(const QString& text, int count)
{
    bool okay = false;
    const int number = text.toInt(&okay);
    if (!okay || number == 0) {
        return -1;
    }
    const int index = number > 0 ? number - 1 : count + number;
    return index >= 0 && index < count ? index : -1;
}

bool parse_ref(const QString& text, int positions, int uvs, int normals,
               VertexRef& ref)
{
    const QStringList parts = text.split(QLatin1Char('/'));
    if (parts.isEmpty() || parts.size() > 3) {
        return false;
    }
    ref.position = obj_index(parts[0], positions);
    if (ref.position < 0) {
        return false;
    }
    if (parts.size() >= 2 && !parts[1].isEmpty()) {
        ref.uv = obj_index(parts[1], uvs);
        if (ref.uv < 0) {
            return false;
        }
    }
    if (parts.size() == 3 && !parts[2].isEmpty()) {
        ref.normal = obj_index(parts[2], normals);
        if (ref.normal < 0) {
            return false;
        }
    }
    return true;
}

QJsonArray vector_json(const QVector3D& point)
{
    return QJsonArray{point.x(), point.y(), point.z()};
}

void append_view(QJsonArray& views, qsizetype offset, qsizetype length,
                 int target = 0, int stride = 0)
{
    QJsonObject view{{QStringLiteral("buffer"), 0},
                     {QStringLiteral("byteOffset"), qint64(offset)},
                     {QStringLiteral("byteLength"), qint64(length)}};
    if (target) {
        view[QStringLiteral("target")] = target;
    }
    if (stride) {
        view[QStringLiteral("byteStride")] = stride;
    }
    views.append(view);
}

void append_accessor(QJsonArray& accessors, int view, int offset,
                     int componentType, int count, const QString& type,
                     const QVector3D* minimum = nullptr,
                     const QVector3D* maximum = nullptr)
{
    QJsonObject accessor{{QStringLiteral("bufferView"), view},
                         {QStringLiteral("byteOffset"), offset},
                         {QStringLiteral("componentType"), componentType},
                         {QStringLiteral("count"), count},
                         {QStringLiteral("type"), type}};
    if (minimum && maximum) {
        accessor[QStringLiteral("min")] = vector_json(*minimum);
        accessor[QStringLiteral("max")] = vector_json(*maximum);
    }
    accessors.append(accessor);
}

struct ImageData
{
    QByteArray bytes;
    QString mime;
    bool alpha = false;
    QString error;
};

ImageData read_image(const QString& path, qint64 remaining)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > remaining) {
        return {{}, {}, false,
                QObject::tr("Could not read OBJ texture: %1").arg(path)};
    }
    QByteArray bytes = file.readAll();
    const QImage image = QImage::fromData(bytes);
    if (image.isNull()) {
        return {{}, {}, false,
                QObject::tr("Unsupported OBJ texture: %1").arg(path)};
    }
    QString mime;
    if (bytes.startsWith("\x89PNG\r\n\x1a\n")) {
        mime = QStringLiteral("image/png");
    } else if (bytes.startsWith("\xff\xd8\xff")) {
        mime = QStringLiteral("image/jpeg");
    } else {
        bytes.clear();
        QBuffer output(&bytes);
        if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG")) {
            return {{}, {}, false,
                    QObject::tr("Could not encode OBJ texture: %1").arg(path)};
        }
        mime = QStringLiteral("image/png");
    }
    if (bytes.size() > remaining) {
        return {{}, {}, false, QObject::tr("Model exceeds 128 MB after packing")};
    }
    return {bytes, mime, image.hasAlphaChannel(), {}};
}

ImageData pack_material_maps(const Material& material, qint64 remaining)
{
    QImage roughness;
    QImage metallic;
    QImage occlusion;
    const auto load = [](const QString& path, QImage& output) {
        if (path.isEmpty()) {
            return true;
        }
        output = QImage(path).convertToFormat(QImage::Format_Grayscale8);
        return !output.isNull();
    };
    if (!load(material.roughnessPath, roughness)
        || !load(material.metallicPath, metallic)
        || !load(material.occlusionPath, occlusion)) {
        return {{}, {}, false, QObject::tr("Could not read OBJ material map")};
    }
    const QSize size = !roughness.isNull() ? roughness.size()
                       : !metallic.isNull() ? metallic.size()
                                            : occlusion.size();
    if (size.isEmpty()) {
        return {{}, {}, false, QObject::tr("OBJ material map is empty")};
    }
    if (!roughness.isNull() && roughness.size() != size) {
        roughness = roughness.scaled(size);
    }
    if (!metallic.isNull() && metallic.size() != size) {
        metallic = metallic.scaled(size);
    }
    if (!occlusion.isNull() && occlusion.size() != size) {
        occlusion = occlusion.scaled(size);
    }
    QImage packed(size, QImage::Format_RGB888);
    if (packed.isNull()) {
        return {{}, {}, false, QObject::tr("OBJ material map is too large")};
    }
    for (int y = 0; y < size.height(); ++y) {
        uchar* output = packed.scanLine(y);
        const uchar* r = roughness.isNull() ? nullptr : roughness.constScanLine(y);
        const uchar* m = metallic.isNull() ? nullptr : metallic.constScanLine(y);
        const uchar* o = occlusion.isNull() ? nullptr : occlusion.constScanLine(y);
        for (int x = 0; x < size.width(); ++x) {
            output[3 * x] = o ? o[x] : 255;
            output[3 * x + 1] = r ? r[x] : 255;
            output[3 * x + 2] = m ? m[x] : 0;
        }
    }
    QByteArray bytes;
    QBuffer output(&bytes);
    if (!output.open(QIODevice::WriteOnly) || !packed.save(&output, "PNG")
        || bytes.size() > remaining) {
        return {{}, {}, false, QObject::tr("Model exceeds 128 MB after packing")};
    }
    return {bytes, QStringLiteral("image/png"), false, {}};
}
} // namespace

GltfImportResult import_obj(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > kmaxGlbBytes) {
        return {{}, QObject::tr("Could not read .obj file (maximum 128 MB)")};
    }
    const QDir root(QFileInfo(filename).absolutePath());
    QDir resourceRoot = root;
    QDir parent = root;
    if (parent.cdUp() && parent.exists(QStringLiteral("textures"))) {
        resourceRoot = parent;
    }
    QTextStream input(&file);
    QVector<QVector3D> positions;
    QVector<QVector2D> uvs;
    QVector<QVector3D> normals;
    QVector<Material> materials;
    QVector<Primitive> primitives;
    QHash<QString, int> materialNames;
    int current = ensure_material(QString(), materials, primitives, materialNames);
    qint64 geometryBytes = 0;
    int lineNumber = 0;
    while (!input.atEnd()) {
        QString line = input.readLine();
        const qsizetype comment = line.indexOf(QLatin1Char('#'));
        if (comment >= 0) {
            line.truncate(comment);
        }
        line = line.trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const qsizetype space = command_end(line);
        if (space < 0) {
            continue;
        }
        const QString command = line.left(space);
        const QString value = line.mid(space + 1).trimmed();
        if (command == QLatin1String("mtllib")) {
            QStringList libraries{value};
            if (asset_path(resourceRoot, root, value).isEmpty()) {
                libraries = value.simplified().split(QLatin1Char(' '));
            }
            for (const QString& library : libraries) {
                const QString path = asset_path(resourceRoot, root, library);
                if (path.isEmpty()) {
                    return {{}, QObject::tr("OBJ material file is missing: %1")
                                    .arg(library)};
                }
                const QString error = parse_mtl(path, materials, primitives,
                                                materialNames);
                if (!error.isEmpty()) {
                    return {{}, error};
                }
            }
        } else if (command == QLatin1String("usemtl")) {
            current = ensure_material(value, materials, primitives, materialNames);
        } else if (command == QLatin1String("v")
                   || command == QLatin1String("vn")
                   || command == QLatin1String("vt")) {
            const QStringList numbers = value.simplified().split(QLatin1Char(' '));
            const int needed = command == QLatin1String("vt") ? 2 : 3;
            float coordinates[3]{};
            bool valid = numbers.size() >= needed;
            for (int i = 0; valid && i < needed; ++i) {
                bool okay = false;
                coordinates[i] = numbers.at(i).toFloat(&okay);
                valid = okay && std::isfinite(coordinates[i]);
            }
            if (!valid) {
                return {{}, QObject::tr("Invalid OBJ vertex at line %1")
                                .arg(lineNumber)};
            }
            if (command == QLatin1String("v")) {
                positions.append(QVector3D(coordinates[0], coordinates[1],
                                           coordinates[2]));
            } else if (command == QLatin1String("vn")) {
                normals.append(QVector3D(coordinates[0], coordinates[1],
                                         coordinates[2]));
            } else {
                uvs.append(QVector2D(coordinates[0], 1.0f - coordinates[1]));
            }
        } else if (command == QLatin1String("f")) {
            const QStringList tokens = value.simplified().split(QLatin1Char(' '));
            if (tokens.size() < 3 || tokens.size() > 10000) {
                return {{}, QObject::tr("Invalid OBJ face at line %1")
                                .arg(lineNumber)};
            }
            QVector<VertexRef> face;
            face.reserve(tokens.size());
            for (const QString& token : tokens) {
                VertexRef ref;
                if (!parse_ref(token, int(positions.size()), int(uvs.size()),
                               int(normals.size()), ref)) {
                    return {{}, QObject::tr("Invalid OBJ face index at line %1")
                                    .arg(lineNumber)};
                }
                face.append(ref);
            }
            Primitive& primitive = primitives[current];
            for (int i = 1; i + 1 < face.size(); ++i) {
                const VertexRef refs[3] = {face[0], face[i], face[i + 1]};
                QVector3D normal = QVector3D::crossProduct(
                    positions[refs[1].position] - positions[refs[0].position],
                    positions[refs[2].position] - positions[refs[0].position]);
                if (normal.lengthSquared() < 1e-20f) {
                    continue;
                }
                normal.normalize();
                const bool knownNormals = refs[0].normal >= 0
                                          && refs[1].normal >= 0
                                          && refs[2].normal >= 0;
                for (const VertexRef& ref : refs) {
                    quint32 index = 0;
                    const auto known = knownNormals
                        ? primitive.knownVertices.constFind(ref)
                        : primitive.knownVertices.cend();
                    if (knownNormals && known != primitive.knownVertices.cend()) {
                        index = *known;
                    } else {
                        if (geometryBytes > kmaxGlbBytes - 36) {
                            return {{}, QObject::tr("OBJ model exceeds 128 MB")};
                        }
                        index = quint32(primitive.vertices.size());
                        Vertex vertex{positions[ref.position],
                                      ref.normal >= 0 ? normals[ref.normal] : normal,
                                      ref.uv >= 0 ? uvs[ref.uv] : QVector2D()};
                        primitive.vertices.append(vertex);
                        for (int axis = 0; axis < 3; ++axis) {
                            primitive.minimum[axis] = std::min(
                                primitive.minimum[axis], vertex.position[axis]);
                            primitive.maximum[axis] = std::max(
                                primitive.maximum[axis], vertex.position[axis]);
                        }
                        if (knownNormals) {
                            primitive.knownVertices.insert(ref, index);
                        }
                        geometryBytes += 32;
                    }
                    if (geometryBytes > kmaxGlbBytes - 4) {
                        return {{}, QObject::tr("OBJ model exceeds 128 MB")};
                    }
                    primitive.indices.append(index);
                    geometryBytes += 4;
                }
            }
        }
    }
    if (geometryBytes == 0) {
        return {{}, QObject::tr("The .obj file contains no faces")};
    }
    for (int i = 0; i < materials.size(); ++i) {
        Material& material = materials[i];
        if (primitives[i].indices.isEmpty() || material.texture.isEmpty()) {
            continue;
        }
        material.basePath = asset_path(resourceRoot, material.textureDir,
                                       material.texture);
        if (material.basePath.isEmpty()) {
            return {{}, QObject::tr("OBJ texture is missing: %1")
                            .arg(material.texture)};
        }
    }
    discover_textures(root, materials, primitives);

    QByteArray binary;
    QJsonArray views;
    QJsonArray accessors;
    QJsonArray meshPrimitives;
    {
        QDataStream stream(&binary, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        for (int materialIndex = 0; materialIndex < primitives.size(); ++materialIndex) {
            const Primitive& primitive = primitives[materialIndex];
            if (primitive.indices.isEmpty()) {
                continue;
            }
            const qsizetype vertexOffset = binary.size();
            for (const Vertex& vertex : primitive.vertices) {
                stream << vertex.position.x() << vertex.position.y()
                       << vertex.position.z() << vertex.normal.x()
                       << vertex.normal.y() << vertex.normal.z()
                       << vertex.uv.x() << vertex.uv.y();
            }
            const int vertexView = int(views.size());
            append_view(views, vertexOffset, binary.size() - vertexOffset,
                        34962, 32);
            const int positionAccessor = int(accessors.size());
            append_accessor(accessors, vertexView, 0, 5126,
                            int(primitive.vertices.size()), QStringLiteral("VEC3"),
                            &primitive.minimum, &primitive.maximum);
            const int normalAccessor = int(accessors.size());
            append_accessor(accessors, vertexView, 12, 5126,
                            int(primitive.vertices.size()), QStringLiteral("VEC3"));
            const int uvAccessor = int(accessors.size());
            append_accessor(accessors, vertexView, 24, 5126,
                            int(primitive.vertices.size()), QStringLiteral("VEC2"));
            const qsizetype indexOffset = binary.size();
            for (quint32 index : primitive.indices) {
                stream << index;
            }
            const int indexView = int(views.size());
            append_view(views, indexOffset, binary.size() - indexOffset, 34963);
            const int indexAccessor = int(accessors.size());
            append_accessor(accessors, indexView, 0, 5125,
                            int(primitive.indices.size()), QStringLiteral("SCALAR"));
            meshPrimitives.append(QJsonObject{
                {QStringLiteral("attributes"),
                 QJsonObject{{QStringLiteral("POSITION"), positionAccessor},
                             {QStringLiteral("NORMAL"), normalAccessor},
                             {QStringLiteral("TEXCOORD_0"), uvAccessor}}},
                {QStringLiteral("indices"), indexAccessor},
                {QStringLiteral("material"), materialIndex}});
        }
    }

    QJsonArray jsonMaterials;
    QJsonArray images;
    QJsonArray textures;
    QHash<QString, int> imageIndices;
    QHash<QString, bool> imageAlpha;
    QString textureError;
    const auto store_image = [&](const QString& key, ImageData image) {
        const int cached = imageIndices.value(key, -1);
        if (cached >= 0) {
            return cached;
        }
        if (!image.error.isEmpty()) {
            textureError = image.error;
            return -1;
        }
        if (image.bytes.size() > kmaxGlbBytes - binary.size() - 4) {
            textureError = QObject::tr("Model exceeds 128 MB after packing");
            return -1;
        }
        const qsizetype offset = binary.size();
        const qsizetype length = image.bytes.size();
        binary.append(image.bytes);
        while (binary.size() % 4) {
            binary.append('\0');
        }
        const int view = int(views.size());
        append_view(views, offset, length);
        const int imageIndex = int(images.size());
        images.append(QJsonObject{{QStringLiteral("bufferView"), view},
                                  {QStringLiteral("mimeType"), image.mime}});
        const int textureIndex = int(textures.size());
        textures.append(QJsonObject{{QStringLiteral("source"), imageIndex}});
        imageIndices.insert(key, textureIndex);
        imageAlpha.insert(key, image.alpha);
        return textureIndex;
    };
    const auto load_texture = [&](const QString& path) {
        const int cached = imageIndices.value(path, -1);
        return cached >= 0 ? cached
                           : store_image(path, read_image(
                               path, kmaxGlbBytes - binary.size()));
    };
    for (int i = 0; i < materials.size(); ++i) {
        const Material& material = materials[i];
        QJsonObject pbr{{QStringLiteral("baseColorFactor"),
                         QJsonArray{material.color.x(), material.color.y(),
                                    material.color.z(), material.opacity}},
                        {QStringLiteral("metallicFactor"),
                         material.metallicPath.isEmpty() ? 0 : 1},
                        {QStringLiteral("roughnessFactor"), 1}};
        bool alpha = material.opacity < 1.0f;
        if (!primitives[i].indices.isEmpty() && !material.basePath.isEmpty()) {
            const int textureIndex = load_texture(material.basePath);
            if (textureIndex < 0) {
                return {{}, textureError};
            }
            alpha = alpha || imageAlpha.value(material.basePath);
            pbr[QStringLiteral("baseColorTexture")]
                = QJsonObject{{QStringLiteral("index"), textureIndex}};
        }
        QJsonObject gltfMaterial{{QStringLiteral("pbrMetallicRoughness"), pbr},
                                 {QStringLiteral("doubleSided"), true}};
        if (!primitives[i].indices.isEmpty()) {
            if (!material.roughnessPath.isEmpty()
                || !material.metallicPath.isEmpty()
                || !material.occlusionPath.isEmpty()) {
                const QString key = QStringLiteral("packed:") + material.name;
                const int index = store_image(
                    key, pack_material_maps(material,
                                            kmaxGlbBytes - binary.size()));
                if (index < 0) {
                    return {{}, textureError};
                }
                if (!material.roughnessPath.isEmpty()
                    || !material.metallicPath.isEmpty()) {
                    pbr[QStringLiteral("metallicRoughnessTexture")]
                        = QJsonObject{{QStringLiteral("index"), index}};
                }
                if (!material.occlusionPath.isEmpty()) {
                    gltfMaterial[QStringLiteral("occlusionTexture")]
                        = QJsonObject{{QStringLiteral("index"), index}};
                }
            }
            if (!material.normalPath.isEmpty()) {
                const int index = load_texture(material.normalPath);
                if (index < 0) {
                    return {{}, textureError};
                }
                gltfMaterial[QStringLiteral("normalTexture")]
                    = QJsonObject{{QStringLiteral("index"), index}};
            }
            if (!material.emissivePath.isEmpty()) {
                const int index = load_texture(material.emissivePath);
                if (index < 0) {
                    return {{}, textureError};
                }
                gltfMaterial[QStringLiteral("emissiveTexture")]
                    = QJsonObject{{QStringLiteral("index"), index}};
                gltfMaterial[QStringLiteral("emissiveFactor")]
                    = QJsonArray{1, 1, 1};
            }
        }
        gltfMaterial[QStringLiteral("pbrMetallicRoughness")] = pbr;
        if (!material.name.isEmpty()) {
            gltfMaterial[QStringLiteral("name")] = material.name;
        }
        if (alpha) {
            gltfMaterial[QStringLiteral("alphaMode")] = QStringLiteral("BLEND");
        }
        jsonMaterials.append(gltfMaterial);
    }

    QJsonObject json{
        {QStringLiteral("asset"),
         QJsonObject{{QStringLiteral("version"), QStringLiteral("2.0")},
                     {QStringLiteral("generator"), QStringLiteral("Familiar OBJ importer")}}},
        {QStringLiteral("scene"), 0},
        {QStringLiteral("scenes"), QJsonArray{QJsonObject{{QStringLiteral("nodes"),
                                                           QJsonArray{0}}}}},
        {QStringLiteral("nodes"), QJsonArray{QJsonObject{{QStringLiteral("mesh"), 0}}}},
        {QStringLiteral("meshes"),
         QJsonArray{QJsonObject{{QStringLiteral("primitives"), meshPrimitives}}}},
        {QStringLiteral("materials"), jsonMaterials},
        {QStringLiteral("buffers"),
         QJsonArray{QJsonObject{{QStringLiteral("byteLength"), binary.size()}}}},
        {QStringLiteral("bufferViews"), views},
        {QStringLiteral("accessors"), accessors},
        {QStringLiteral("images"), images},
        {QStringLiteral("textures"), textures}};
    QByteArray jsonBytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    while (jsonBytes.size() % 4) {
        jsonBytes.append(' ');
    }
    if (binary.size() + jsonBytes.size() + 28 > kmaxGlbBytes) {
        return {{}, QObject::tr("OBJ model exceeds 128 MB after packing")};
    }
    QByteArray glb;
    QDataStream output(&glb, QIODevice::WriteOnly);
    output.setByteOrder(QDataStream::LittleEndian);
    output << quint32(0x46546C67) << quint32(2)
           << quint32(28 + jsonBytes.size() + binary.size())
           << quint32(jsonBytes.size()) << quint32(0x4E4F534A);
    output.writeRawData(jsonBytes.constData(), jsonBytes.size());
    output << quint32(binary.size()) << quint32(0x004E4942);
    output.writeRawData(binary.constData(), binary.size());
    return {glb, {}};
}

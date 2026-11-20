#include "model_preview.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QtEndian>

namespace {
constexpr int ksize = 400;

quint32 u32(const char* bytes)
{
    return qFromLittleEndian<quint32>(bytes);
}

struct Accessor
{
    const char* data = nullptr;
    int count = 0;
    int stride = 0;
    int components = 0;

    bool valid() const { return data && count > 0; }
    float value(int index, int component) const
    {
        float result;
        std::memcpy(&result, data + index * stride + component * 4, 4);
        return result;
    }
};

Accessor accessor(const QJsonObject& json, int index, const char* bin,
                  qsizetype binSize, int components)
{
    const QJsonArray accessors = json.value("accessors").toArray();
    const QJsonArray views = json.value("bufferViews").toArray();
    if (index < 0 || index >= accessors.size()) {
        return {};
    }
    const QJsonObject a = accessors.at(index).toObject();
    const int viewIndex = a.value("bufferView").toInt(-1);
    if (viewIndex < 0 || viewIndex >= views.size()
        || a.value("componentType").toInt() != 5126
        || a.value("type").toString() != (components == 3 ? "VEC3" : "VEC2")) {
        return {};
    }
    const QJsonObject view = views.at(viewIndex).toObject();
    const qint64 count = a.value("count").toInteger();
    const qint64 stride = view.value("byteStride").toInt(components * 4);
    const qint64 offset = view.value("byteOffset").toInteger()
                          + a.value("byteOffset").toInteger();
    const qint64 viewEnd = view.value("byteOffset").toInteger()
                           + view.value("byteLength").toInteger();
    if (count <= 0 || count > 5000000 || stride < components * 4
        || stride > 1024 || offset < 0 || viewEnd > binSize
        || offset + (count - 1) * stride + components * 4 > viewEnd) {
        return {};
    }
    return {bin + offset, int(count), int(stride), components};
}

struct IndexAccessor
{
    const char* data = nullptr;
    int count = 0;
    int stride = 0;
    int componentSize = 0;

    bool valid() const { return data && count >= 3; }
    quint32 value(int index) const
    {
        const char* bytes = data + index * stride;
        if (componentSize == 1) {
            return quint8(*bytes);
        }
        if (componentSize == 2) {
            return qFromLittleEndian<quint16>(bytes);
        }
        return qFromLittleEndian<quint32>(bytes);
    }
};

IndexAccessor indices(const QJsonObject& json, int index, const char* bin,
                      qsizetype binSize)
{
    const QJsonArray accessors = json.value("accessors").toArray();
    const QJsonArray views = json.value("bufferViews").toArray();
    if (index < 0 || index >= accessors.size()) {
        return {};
    }
    const QJsonObject a = accessors.at(index).toObject();
    const int viewIndex = a.value("bufferView").toInt(-1);
    if (viewIndex < 0 || viewIndex >= views.size()
        || a.value("type").toString() != QStringLiteral("SCALAR")) {
        return {};
    }
    const int type = a.value("componentType").toInt();
    const int size = type == 5121 ? 1 : type == 5123 ? 2 : type == 5125 ? 4 : 0;
    const QJsonObject view = views.at(viewIndex).toObject();
    const qint64 count = a.value("count").toInteger();
    const qint64 stride = view.value("byteStride").toInt(size);
    const qint64 offset = view.value("byteOffset").toInteger()
                          + a.value("byteOffset").toInteger();
    const qint64 viewEnd = view.value("byteOffset").toInteger()
                           + view.value("byteLength").toInteger();
    if (!size || count < 3 || count > 20000000 || stride < size
        || stride > 1024 || offset < 0 || viewEnd > binSize
        || offset + (count - 1) * stride + size > viewEnd) {
        return {};
    }
    return {bin + offset, int(count), int(stride), size};
}

QImage base_color_texture(const QJsonObject& json, const char* bin,
                          qsizetype binSize, const QJsonObject& primitive)
{
    const QJsonArray materials = json.value("materials").toArray();
    const QJsonArray textures = json.value("textures").toArray();
    const QJsonArray images = json.value("images").toArray();
    const QJsonArray views = json.value("bufferViews").toArray();
    const int materialIndex = primitive.value("material").toInt(-1);
    if (materialIndex < 0 || materialIndex >= materials.size()) {
        return {};
    }
    const QJsonObject pbr = materials.at(materialIndex).toObject()
                                .value("pbrMetallicRoughness").toObject();
    const int textureIndex = pbr.value("baseColorTexture").toObject()
                                 .value("index").toInt(-1);
    if (textureIndex < 0 || textureIndex >= textures.size()) {
        const QJsonArray factor = pbr.value("baseColorFactor").toArray();
        if (factor.size() < 3) {
            return {};
        }
        QImage color(1, 1, QImage::Format_RGB32);
        color.fill(QColor::fromRgbF(
            float(std::clamp(factor.at(0).toDouble(), 0.0, 1.0)),
            float(std::clamp(factor.at(1).toDouble(), 0.0, 1.0)),
            float(std::clamp(factor.at(2).toDouble(), 0.0, 1.0))));
        return color;
    }
    const int imageIndex = textures.at(textureIndex).toObject()
                               .value("source").toInt(-1);
    if (imageIndex < 0 || imageIndex >= images.size()) {
        return {};
    }
    const int viewIndex = images.at(imageIndex).toObject()
                              .value("bufferView").toInt(-1);
    if (viewIndex < 0 || viewIndex >= views.size()) {
        return {};
    }
    const QJsonObject view = views.at(viewIndex).toObject();
    const qint64 offset = view.value("byteOffset").toInteger();
    const qint64 length = view.value("byteLength").toInteger();
    if (offset < 0 || length <= 0 || offset + length > binSize) {
        return {};
    }
    return QImage::fromData(reinterpret_cast<const uchar*>(bin + offset),
                            int(length)).convertToFormat(QImage::Format_RGB32);
}

struct Cloud
{
    Accessor position;
    Accessor uv;
    IndexAccessor index;
    QImage texture;
};

struct ScreenVertex
{
    float x;
    float y;
    float z;
    float u;
    float v;
};

QRgb texture_color(const QImage& texture, float u, float v)
{
    if (texture.isNull() || !std::isfinite(u) || !std::isfinite(v)) {
        return qRgb(194, 207, 222);
    }
    const int tx = std::clamp(int((u - std::floor(u)) * float(texture.width())),
                              0, texture.width() - 1);
    const int ty = std::clamp(int((v - std::floor(v))
                                  * float(texture.height())),
                              0, texture.height() - 1);
    return texture.pixel(tx, ty);
}
} // namespace

ModelBounds model_bounds(const QByteArray& glb)
{
    if (glb.size() < 20 || std::memcmp(glb.constData(), "glTF", 4) != 0
        || u32(glb.constData() + 4) != 2
        || u32(glb.constData() + 8) != quint32(glb.size())) {
        return {};
    }
    const quint32 jsonSize = u32(glb.constData() + 12);
    if (u32(glb.constData() + 16) != 0x4e4f534a
        || jsonSize > quint64(glb.size() - 20)) {
        return {};
    }
    const QJsonObject json = QJsonDocument::fromJson(glb.mid(20, jsonSize)).object();
    const QJsonArray accessors = json.value("accessors").toArray();
    QVector3D low(std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max());
    QVector3D high(-low.x(), -low.y(), -low.z());
    bool found = false;
    for (const QJsonValue& mesh : json.value("meshes").toArray()) {
        for (const QJsonValue& primitive : mesh.toObject().value("primitives").toArray()) {
            const int index = primitive.toObject().value("attributes").toObject()
                                  .value("POSITION").toInt(-1);
            if (index < 0 || index >= accessors.size()) {
                continue;
            }
            const QJsonObject position = accessors.at(index).toObject();
            const QJsonArray minimum = position.value("min").toArray();
            const QJsonArray maximum = position.value("max").toArray();
            if (minimum.size() != 3 || maximum.size() != 3) {
                continue;
            }
            found = true;
            for (int axis = 0; axis < 3; ++axis) {
                low[axis] = std::min(low[axis], float(minimum.at(axis).toDouble()));
                high[axis] = std::max(high[axis], float(maximum.at(axis).toDouble()));
            }
        }
    }
    if (!found) {
        return {};
    }
    const float extent = std::max({high.x() - low.x(), high.y() - low.y(),
                                   high.z() - low.z()});
    if (!std::isfinite(extent) || extent <= 0) {
        return {};
    }
    return {(low + high) / 2, extent};
}

QImage render_model_preview(const QByteArray& glb)
{
    if (glb.size() < 28 || std::memcmp(glb.constData(), "glTF", 4) != 0
        || u32(glb.constData() + 4) != 2
        || u32(glb.constData() + 8) != quint32(glb.size())) {
        return {};
    }
    qsizetype offset = 12;
    QJsonObject json;
    const char* bin = nullptr;
    qsizetype binSize = 0;
    while (offset + 8 <= glb.size()) {
        const quint32 length = u32(glb.constData() + offset);
        const quint32 type = u32(glb.constData() + offset + 4);
        offset += 8;
        if (length > quint64(glb.size() - offset)) {
            return {};
        }
        if (type == 0x4e4f534a) {
            json = QJsonDocument::fromJson(glb.mid(offset, length)).object();
        } else if (type == 0x004e4942 && !bin) {
            bin = glb.constData() + offset;
            binSize = length;
        }
        offset += length;
    }
    if (!bin || json.isEmpty()) {
        return {};
    }

    std::vector<Cloud> clouds;
    QVector3D low(std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max());
    QVector3D high(-low.x(), -low.y(), -low.z());
    const QJsonArray meshes = json.value("meshes").toArray();
    for (const QJsonValue& meshValue : meshes) {
        for (const QJsonValue& primitiveValue : meshValue.toObject()
                 .value("primitives").toArray()) {
            const QJsonObject primitive = primitiveValue.toObject();
            const QJsonObject attrs = primitive.value("attributes").toObject();
            Cloud cloud;
            cloud.position = accessor(json, attrs.value("POSITION").toInt(-1),
                                      bin, binSize, 3);
            if (!cloud.position.valid()) {
                continue;
            }
            cloud.uv = accessor(json, attrs.value("TEXCOORD_0").toInt(-1),
                                bin, binSize, 2);
            if (primitive.value("mode").toInt(4) != 4) {
                continue;
            }
            cloud.index = indices(json, primitive.value("indices").toInt(-1),
                                   bin, binSize);
            if (primitive.contains("indices") && !cloud.index.valid()) {
                continue;
            }
            cloud.texture = base_color_texture(json, bin, binSize, primitive);
            const QJsonObject position = json.value("accessors").toArray()
                                             .at(attrs.value("POSITION").toInt())
                                             .toObject();
            const QJsonArray minimum = position.value("min").toArray();
            const QJsonArray maximum = position.value("max").toArray();
            if (minimum.size() == 3 && maximum.size() == 3) {
                for (int axis = 0; axis < 3; ++axis) {
                    low[axis] = std::min(low[axis], float(minimum.at(axis).toDouble()));
                    high[axis] = std::max(high[axis], float(maximum.at(axis).toDouble()));
                }
            } else {
                for (int i = 0; i < cloud.position.count; ++i) {
                    for (int axis = 0; axis < 3; ++axis) {
                        low[axis] = std::min(low[axis], cloud.position.value(i, axis));
                        high[axis] = std::max(high[axis], cloud.position.value(i, axis));
                    }
                }
            }
            clouds.push_back(std::move(cloud));
        }
    }
    if (clouds.empty()) {
        return {};
    }
    const QVector3D center = (low + high) / 2;
    const float extent = std::max({high.x() - low.x(), high.y() - low.y(),
                                   high.z() - low.z()});
    if (!std::isfinite(extent) || extent <= 0) {
        return {};
    }

    QImage result(ksize, ksize, QImage::Format_RGB32);
    result.fill(QColor("#28313d"));
    std::vector<float> depth(ksize * ksize,
                             -std::numeric_limits<float>::infinity());
    QMatrix4x4 rotation;
    rotation.rotate(-15, 1, 0, 0);
    rotation.rotate(25, 0, 1, 0);
    const float scale = (ksize * 0.78f) / extent;
    for (const Cloud& cloud : clouds) {
        std::vector<ScreenVertex> vertices;
        vertices.reserve(size_t(cloud.position.count));
        for (int i = 0; i < cloud.position.count; ++i) {
            const QVector3D point(cloud.position.value(i, 0),
                                  cloud.position.value(i, 1),
                                  cloud.position.value(i, 2));
            const QVector3D rotated = rotation.map(point - center);
            vertices.push_back({ksize / 2.0f + rotated.x() * scale,
                                ksize / 2.0f - rotated.y() * scale,
                                rotated.z(),
                                cloud.uv.valid() && i < cloud.uv.count
                                    ? cloud.uv.value(i, 0) : 0,
                                cloud.uv.valid() && i < cloud.uv.count
                                    ? cloud.uv.value(i, 1) : 0});
        }
        const int indexCount = cloud.index.valid()
                                   ? cloud.index.count : cloud.position.count;
        for (int triangle = 0; triangle + 2 < indexCount; triangle += 3) {
            const quint32 i0 = cloud.index.valid() ? cloud.index.value(triangle)
                                                    : quint32(triangle);
            const quint32 i1 = cloud.index.valid() ? cloud.index.value(triangle + 1)
                                                    : quint32(triangle + 1);
            const quint32 i2 = cloud.index.valid() ? cloud.index.value(triangle + 2)
                                                    : quint32(triangle + 2);
            if (i0 >= quint32(vertices.size()) || i1 >= quint32(vertices.size())
                || i2 >= quint32(vertices.size())) {
                continue;
            }
            const ScreenVertex& a = vertices[i0];
            const ScreenVertex& b = vertices[i1];
            const ScreenVertex& c = vertices[i2];
            if (!std::isfinite(a.x) || !std::isfinite(a.y)
                || !std::isfinite(b.x) || !std::isfinite(b.y)
                || !std::isfinite(c.x) || !std::isfinite(c.y)) {
                continue;
            }
            const float area = (b.x - a.x) * (c.y - a.y)
                               - (b.y - a.y) * (c.x - a.x);
            if (!std::isfinite(area) || std::abs(area) < 0.00001f) {
                continue;
            }
            const float left = std::min({a.x, b.x, c.x});
            const float right = std::max({a.x, b.x, c.x});
            const float top = std::min({a.y, b.y, c.y});
            const float bottom = std::max({a.y, b.y, c.y});
            if (right < 0 || bottom < 0
                || left >= float(ksize) || top >= float(ksize)) {
                continue;
            }
            const int minX = int(std::floor(std::max(0.0f, left)));
            const int maxX = int(std::ceil(std::min(float(ksize - 1), right)));
            const int minY = int(std::floor(std::max(0.0f, top)));
            const int maxY = int(std::ceil(std::min(float(ksize - 1), bottom)));
            if (minX > maxX || minY > maxY) {
                continue;
            }
            const float inverseArea = 1 / area;
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    const float px = float(x) + 0.5f;
                    const float py = float(y) + 0.5f;
                    const float wa = ((b.x - px) * (c.y - py)
                                      - (b.y - py) * (c.x - px)) * inverseArea;
                    const float wb = ((c.x - px) * (a.y - py)
                                      - (c.y - py) * (a.x - px)) * inverseArea;
                    const float wc = 1 - wa - wb;
                    if (wa < 0 || wb < 0 || wc < 0) {
                        continue;
                    }
                    const size_t pixel = size_t(y * ksize + x);
                    const float z = wa * a.z + wb * b.z + wc * c.z;
                    if (z <= depth[pixel]) {
                        continue;
                    }
                    depth[pixel] = z;
                    result.setPixel(x, y,
                                    texture_color(cloud.texture,
                                                  wa * a.u + wb * b.u + wc * c.u,
                                                  wa * a.v + wb * b.v + wc * c.v));
                }
            }
        }
    }
    return result;
}

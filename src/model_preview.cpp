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
        return {};
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
    QImage texture;
};
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
        const int step = std::max(1, cloud.position.count / 1000000);
        for (int i = 0; i < cloud.position.count; i += step) {
            const QVector3D point(cloud.position.value(i, 0),
                                  cloud.position.value(i, 1),
                                  cloud.position.value(i, 2));
            const QVector3D rotated = rotation.map(point - center);
            const int x = int(std::lround(ksize / 2.0f + rotated.x() * scale));
            const int y = int(std::lround(ksize / 2.0f - rotated.y() * scale));
            if (x < 1 || x >= ksize - 1 || y < 1 || y >= ksize - 1) {
                continue;
            }
            QRgb color = qRgb(194, 207, 222);
            if (!cloud.texture.isNull() && cloud.uv.valid()
                && i < cloud.uv.count) {
                const float u = cloud.uv.value(i, 0);
                const float v = cloud.uv.value(i, 1);
                if (std::isfinite(u) && std::isfinite(v)) {
                    const int tx = std::clamp(int((u - std::floor(u))
                                                  * float(cloud.texture.width())),
                                              0, cloud.texture.width() - 1);
                    const int ty = std::clamp(int((1 - (v - std::floor(v)))
                                                  * float(cloud.texture.height())),
                                              0, cloud.texture.height() - 1);
                    color = cloud.texture.pixel(tx, ty);
                }
            }
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int px = x + dx, py = y + dy;
                    const size_t pixel = size_t(py * ksize + px);
                    if (rotated.z() > depth[pixel]) {
                        depth[pixel] = rotated.z();
                        result.setPixel(px, py, color);
                    }
                }
            }
        }
    }
    return result;
}

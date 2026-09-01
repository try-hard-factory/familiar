#include "fml_archive.h"

#include "canvasscene.h"
#include "fileio.h"
#include "moveitem.h"

#include "miniz.h"
#include "miniz_zip.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <QVariantMap>

#include <cstring>
#include <functional>
#include <optional>

#include "log/log.h"
using namespace familiar::log;

// ============================================================================
// Manifest DTOs and JSON (de)serialization.
//
// Per docs/fml_format_design.md §7.1, Qt JSON types (QJsonDocument/Object/
// Array/Value) never leave this file. Everything outside this translation
// unit deals in the plain structs below - swapping the JSON backend later
// means rewriting only write_manifest()/parse_manifest().
// ============================================================================

namespace {

constexpr int kFormatVersion = 1;
const char kFormatMagic[] = "familiar";
const unsigned char kZipMagic[4] = {'P', 'K', 0x03, 0x04};

// ── Format versioning ────────────────────────────────────────────────
// Documented in docs/fml_format_design.md §6 but never implemented until
// now - mirrors settings.json's analogous mechanism in
// core/settingshandler.cpp (applySettingsMigrations()/
// settingsMigrations()), applied here to manifest.json instead. A
// formatVersion newer than kFormatVersion is still a hard load refusal
// (see the check in parse_manifest() below, unchanged) - unlike
// settings.json, this is a user's actual artwork, not just preferences,
// so silently best-effort-loading a schema we don't fully understand is
// the wrong tradeoff here.
//
// {fromVersion: transform} - migrations[N] rewrites `root` (the parsed
// manifest.json object, before any of its fields are read into Manifest/
// ManifestItem) from formatVersion N to N+1 in place; applyFormatMigrations()
// below runs every migration from the document's own formatVersion up to
// kFormatVersion in ascending order. Empty for now - kFormatVersion has
// never been bumped, so there's nothing real to migrate from yet. Only
// bump kFormatVersion, and add an entry here, for a change that breaks
// reading old data (a renamed/restructured/reinterpreted field) - purely
// additive fields don't need one, since unknown fields are already
// ignored on read (§6, "Незнакомое поле JSON — молча игнорировать").
const QMap<int, std::function<void(QJsonObject&)>>& formatMigrations()
{
    static const QMap<int, std::function<void(QJsonObject&)>> migrations = {
        // {1, [](QJsonObject& root) { ... }},
    };
    return migrations;
}

void applyFormatMigrations(QJsonObject& root, int fromVersion)
{
    const auto& migrations = formatMigrations();
    for (int v = fromVersion; v < kFormatVersion; ++v) {
        auto it = migrations.find(v);
        if (it != migrations.end()) {
            it.value()(root);
        }
    }
    root[QStringLiteral("formatVersion")] = kFormatVersion;
}

struct ManifestItem
{
    QUuid id;
    QString type;
    qreal x = 0;
    qreal y = 0;
    qreal z = 0;
    qreal scale = 1;
    qreal rotation = 0;
    int flip = 1;
    QString image; // empty for non-pixmap items
    QVariantMap data;
};

struct Manifest
{
    int formatVersion = kFormatVersion;
    QString appVersion;
    // The scene's remembered bounding rect (CanvasScene::
    // rememberedBoundingRect()) - empty if the scene never had content.
    // Round-tripped so a project saved with zero items still shows its
    // old "empty space" frame instead of looking brand-new on reload.
    QRectF sceneBoundingRect;
    QList<ManifestItem> items;
};

QByteArray write_manifest(const Manifest& manifest)
{
    QJsonObject root;
    root[QStringLiteral("format")] = QString::fromLatin1(kFormatMagic);
    root[QStringLiteral("formatVersion")] = manifest.formatVersion;
    root[QStringLiteral("appVersion")] = manifest.appVersion;

    QJsonObject sceneObj;
    if (!manifest.sceneBoundingRect.isEmpty()) {
        sceneObj[QStringLiteral("boundingRect")]
            = QJsonArray{manifest.sceneBoundingRect.x(),
                         manifest.sceneBoundingRect.y(),
                         manifest.sceneBoundingRect.width(),
                         manifest.sceneBoundingRect.height()};
    }
    root[QStringLiteral("scene")] = sceneObj;

    QJsonArray items;
    for (const ManifestItem& item : manifest.items) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = item.id.toString(QUuid::WithoutBraces);
        obj[QStringLiteral("type")] = item.type;
        obj[QStringLiteral("x")] = item.x;
        obj[QStringLiteral("y")] = item.y;
        obj[QStringLiteral("z")] = item.z;
        obj[QStringLiteral("scale")] = item.scale;
        obj[QStringLiteral("rotation")] = item.rotation;
        obj[QStringLiteral("flip")] = item.flip;
        if (!item.image.isEmpty()) {
            obj[QStringLiteral("image")] = item.image;
        }
        obj[QStringLiteral("data")] = QJsonObject::fromVariantMap(item.data);
        items.append(obj);
    }
    root[QStringLiteral("items")] = items;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

// Returns nullopt on a fatal problem (not JSON, not a familiar manifest,
// or from a formatVersion we don't understand yet) with `error` set.
// A manifest item with a missing/duplicate id is not fatal - see the
// per-item warning below (docs/fml_format_design.md §5.1).
std::optional<Manifest> parse_manifest(const QByteArray& json, QString& error)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QStringLiteral("Invalid manifest.json: %1")
                    .arg(parseError.errorString());
        FLOG_ERROR(Ch::IO, "{}", error);
        return std::nullopt;
    }
    if (!doc.isObject()) {
        error = QStringLiteral("manifest.json is not a JSON object");
        FLOG_ERROR(Ch::IO, "{}", error);
        return std::nullopt;
    }

    QJsonObject root = doc.object();

    if (root.value(QStringLiteral("format")).toString()
        != QString::fromLatin1(kFormatMagic)) {
        error = QStringLiteral(
            "Not a familiar project file (missing format marker)");
        FLOG_ERROR(Ch::IO, "{}", error);
        return std::nullopt;
    }

    int formatVersion = root.value(QStringLiteral("formatVersion")).toInt(-1);
    if (formatVersion <= 0 || formatVersion > kFormatVersion) {
        error = QStringLiteral("This file was created by a newer version of "
                               "familiar (formatVersion %1)")
                    .arg(formatVersion);
        FLOG_ERROR(Ch::IO, "{}", error);
        return std::nullopt;
    }
    // No-op today (formatVersion is always already kFormatVersion, since
    // it's never been bumped) - see applyFormatMigrations() above for why
    // this is here regardless.
    if (formatVersion < kFormatVersion) {
        applyFormatMigrations(root, formatVersion);
    }

    Manifest manifest;
    manifest.formatVersion = kFormatVersion;
    manifest.appVersion = root.value(QStringLiteral("appVersion")).toString();

    QJsonArray boundingRect = root.value(QStringLiteral("scene"))
                                  .toObject()
                                  .value(QStringLiteral("boundingRect"))
                                  .toArray();
    if (boundingRect.size() == 4) {
        manifest.sceneBoundingRect = QRectF(boundingRect[0].toDouble(),
                                            boundingRect[1].toDouble(),
                                            boundingRect[2].toDouble(),
                                            boundingRect[3].toDouble());
    }

    QSet<QUuid> seenIds;
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        ManifestItem item;

        QUuid id = QUuid::fromString(obj.value(QStringLiteral("id")).toString());
        if (id.isNull() || seenIds.contains(id)) {
            FLOG_WARN(Ch::IO,
                      "manifest.json item has a missing or duplicate id; "
                      "generating a new one");
            id = QUuid::createUuid();
        }
        seenIds.insert(id);

        item.id = id;
        item.type = obj.value(QStringLiteral("type")).toString();
        item.x = obj.value(QStringLiteral("x")).toDouble();
        item.y = obj.value(QStringLiteral("y")).toDouble();
        item.z = obj.value(QStringLiteral("z")).toDouble();
        item.scale = obj.value(QStringLiteral("scale")).toDouble(1.0);
        item.rotation = obj.value(QStringLiteral("rotation")).toDouble();
        item.flip = obj.value(QStringLiteral("flip")).toInt(1);
        item.image = obj.value(QStringLiteral("image")).toString();
        item.data = obj.value(QStringLiteral("data")).toObject().toVariantMap();

        manifest.items.append(item);
    }

    return manifest;
}

// RAII for mz_zip_archive. miniz's C API needs an explicit matching
// mz_zip_writer_end()/mz_zip_reader_end() call, and save()/load() below
// each have several early-return error paths; binding the call to scope
// removes the need to get every one of those right by hand. Both _end()
// functions are safe to call even if init (below) was never reached or
// failed - they check the archive's internal state and no-op.
class ZipWriter
{
public:
    ZipWriter() = default;
    ~ZipWriter() { mz_zip_writer_end(&archive_); }

    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;

    bool initHeap() { return mz_zip_writer_init_heap(&archive_, 0, 0); }
    mz_zip_archive* get() { return &archive_; }

private:
    mz_zip_archive archive_{};
};

class ZipReader
{
public:
    ZipReader() = default;
    ~ZipReader() { mz_zip_reader_end(&archive_); }

    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;

    bool initMem(const void* mem, size_t size)
    {
        return mz_zip_reader_init_mem(&archive_, mem, size, 0);
    }
    mz_zip_archive* get() { return &archive_; }

private:
    mz_zip_archive archive_{};
};

bool looks_like_zip(QFile& file)
{
    QByteArray header = file.peek(4);
    return header.size() == 4
           && std::memcmp(header.constData(), kZipMagic, 4) == 0;
}

// ============================================================================
// Legacy pre-zip .fml reader (docs/fml_format_design.md §10). The format
// this reads from was written by fml_file_buffer.h's save_to_file() /
// CanvasScene::fml_payload(); both fml_payload() and CanvasView::addImage()
// were stubs that never actually produced or consumed image data, so no
// real file with content is known to exist for this branch - it exists as
// a safety net, not a verified-working migration path.
// ============================================================================

FmlResult load_legacy(QFile& file, CanvasScene* scene, ThreadedIO* worker)
{
    FmlResult result;

    QDataStream stream(&file);

    qint16 count = 0;
    stream >> count;
    if (stream.status() != QDataStream::Ok || count < 0) {
        result.error = QStringLiteral("Not a valid familiar project file");
        FLOG_ERROR(Ch::IO, "{}", result.error);
        return result;
    }

    if (worker) {
        emit worker->beginProcessing(count);
    }

    for (int i = 0; i < count; ++i) {
        if (worker && worker->canceled) {
            break;
        }

        QPointF scenePos;
        stream >> scenePos;
        qint32 h = 0, w = 0;
        stream >> h >> w;
        QRectF br;
        stream >> br;
        quint16 format = 0;
        stream >> format;
        quint64 size = 0;
        stream >> size;

        if (stream.status() != QDataStream::Ok) {
            result.itemErrors.append(
                QStringLiteral("Item %1: truncated file").arg(i));
            FLOG_WARN(Ch::IO, "load_legacy: item {} truncated file", i);
            break;
        }

        QByteArray compressed = file.read(static_cast<qint64>(size));
        QByteArray raw = qUncompress(compressed);
        if (raw.isEmpty() && size > 0) {
            result.itemErrors.append(
                QStringLiteral("Item %1: could not decompress image data")
                    .arg(i));
            FLOG_WARN(Ch::IO,
                      "load_legacy: item {} could not decompress image data",
                      i);
            if (worker) {
                emit worker->progress(i);
            }
            continue;
        }

        // Let QImage compute its own (aligned) bytesPerLine from w/format
        // instead of trusting whatever the writer used - safer than
        // replaying the original save_to_file() code, which was never
        // actually exercised (see the comment above).
        QImage image(reinterpret_cast<const uchar*>(raw.constData()),
                     w,
                     h,
                     QImage::Format(format));
        image = image.copy(); // detach before `raw` goes out of scope

        QVariantMap itemData;
        itemData[QStringLiteral("type")] = QStringLiteral("pixmap");
        itemData[QStringLiteral("image")] = image;
        itemData[QStringLiteral("x")] = scenePos.x();
        itemData[QStringLiteral("y")] = scenePos.y();
        scene->add_item_later(itemData, false);

        if (worker) {
            emit worker->progress(i);
        }
    }

    return result;
}

} // namespace

// ============================================================================
// FmlArchive
// ============================================================================

FmlResult FmlArchive::save(CanvasScene* scene,
                           const QRectF& canvasRect,
                           const QString& filename,
                           ThreadedIO* worker)
{
    FmlResult result;

    QList<QGraphicsItem*> items = scene->items_for_save();

    ZipWriter zip;
    if (!zip.initHeap()) {
        result.error = QStringLiteral("Could not initialize zip writer");
        FLOG_ERROR(Ch::IO, "FmlArchive::save: {}", result.error);
        return result;
    }

    // Same convention ODF (.odt/.ods/.odp) and EPUB use to make a
    // zip-based format identifiable by content, not just by extension -
    // verified against the OASIS ODF spec's own package rules
    // (https://docs.oasis-open.org/office/v1.2/cs01/OpenDocument-v1.2-cs01-part3.html):
    // a "mimetype" entry, STORED (no compression) and with no extra
    // field, as the very FIRST entry in the archive. That pins its
    // content to a fixed byte offset (30 for the 8-byte name "mimetype"
    // itself, 38 for the content that follows - standard 30-byte PKZIP
    // local file header + an 8-byte filename with no extra field), which
    // is what makes it usable as a `file`/libmagic-style magic-number
    // rule (see packaging/linux/org.tryhardfactory.Familiar.xml's own
    // <magic> block). Purely additive - old readers (including this
    // app's own FmlArchive::load(), which never looks for this entry)
    // just ignore it, so no formatVersion bump. Must be written FIRST,
    // before the item loop below, or the offset promise breaks.
    static const char kMimetypeContent[] = "application/x-familiar-fml";
    if (!mz_zip_writer_add_mem(zip.get(),
                               "mimetype",
                               kMimetypeContent,
                               sizeof(kMimetypeContent) - 1, // no trailing NUL
                               MZ_NO_COMPRESSION)) {
        result.error = QStringLiteral("Could not write mimetype marker");
        FLOG_ERROR(Ch::IO, "FmlArchive::save: {}", result.error);
        return result;
    }

    Manifest manifest;
#ifdef FAMILIAR_VERSION_STRING
    manifest.appVersion = QStringLiteral(FAMILIAR_VERSION_STRING);
#endif
    manifest.sceneBoundingRect = canvasRect;

    if (worker) {
        emit worker->beginProcessing(items.size());
    }

    bool canceled = false;
    for (int i = 0; i < items.size(); ++i) {
        if (worker && worker->canceled) {
            canceled = true;
            break;
        }

        auto* baseItem = dynamic_cast<IBaseItem*>(items[i]);
        Q_ASSERT_X(baseItem, "FmlArchive::save", "item is not an IBaseItem");

        ManifestItem mi;
        mi.id = baseItem->uid();
        mi.type = QString::fromStdString(baseItem->get_type());
        mi.x = items[i]->pos().x();
        mi.y = items[i]->pos().y();
        mi.z = items[i]->zValue();
        mi.scale = items[i]->scale();
        mi.rotation = items[i]->rotation();
        mi.flip = (baseItem->flip() == -1) ? -1 : 1;
        mi.data = baseItem->get_extra_save_data();

        // GifItem checked BEFORE PixmapItem: it IS one (inherits it), so
        // the PixmapItem branch below would also match it - and
        // pixmap_to_bytes() only ever encodes the CURRENTLY DISPLAYED
        // frame as a static raster, silently discarding the animation.
        // Store the original GIF bytes verbatim instead - lossless, and
        // exactly what GifItem's own constructor expects back on load.
        if (auto* gifItem = dynamic_cast<GifItem*>(items[i])) {
            const QByteArray& bytes = gifItem->gif_bytes();
            QString idStr = mi.id.toString(QUuid::WithoutBraces);
            mi.image = QStringLiteral("images/%1.gif").arg(idStr);

            QByteArray archiveName = mi.image.toUtf8();
            if (!mz_zip_writer_add_mem(zip.get(),
                                       archiveName.constData(),
                                       bytes.constData(),
                                       static_cast<size_t>(bytes.size()),
                                       MZ_NO_COMPRESSION)) {
                result.itemErrors.append(
                    QStringLiteral("Could not store image for item %1")
                        .arg(idStr));
                FLOG_WARN(Ch::IO,
                          "FmlArchive::save: could not store gif image for "
                          "item {}",
                          idStr);
            }
        } else if (auto* pixmapItem = dynamic_cast<PixmapItem*>(items[i])) {
            auto [bytes, imgformat] = pixmapItem->pixmap_to_bytes();
            QString idStr = mi.id.toString(QUuid::WithoutBraces);
            mi.image = QStringLiteral("images/%1.%2").arg(idStr, imgformat);

            QByteArray archiveName = mi.image.toUtf8();
            if (!mz_zip_writer_add_mem(zip.get(),
                                       archiveName.constData(),
                                       bytes.constData(),
                                       static_cast<size_t>(bytes.size()),
                                       MZ_NO_COMPRESSION)) {
                result.itemErrors.append(
                    QStringLiteral("Could not store image for item %1")
                        .arg(idStr));
                FLOG_WARN(Ch::IO,
                          "FmlArchive::save: could not store pixmap image "
                          "for item {}",
                          idStr);
            }
        }

        manifest.items.append(mi);

        if (worker) {
            emit worker->progress(i);
        }
    }

    if (canceled) {
        // Don't write anything to disk - the previous file (if any) stays
        // untouched. Mirrors ImageImportSession::run()'s silent-stop-on-cancel
        // convention (empty error, no dialog).
        return result;
    }

    QByteArray manifestJson = write_manifest(manifest);
    if (!mz_zip_writer_add_mem(zip.get(),
                               "manifest.json",
                               manifestJson.constData(),
                               static_cast<size_t>(manifestJson.size()),
                               MZ_DEFAULT_LEVEL)) {
        result.error = QStringLiteral("Could not write manifest.json");
        FLOG_ERROR(Ch::IO, "FmlArchive::save: {}", result.error);
        return result;
    }

    void* buf = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(zip.get(), &buf, &size)) {
        result.error = QStringLiteral("Could not finalize archive");
        FLOG_ERROR(Ch::IO, "FmlArchive::save: {}", result.error);
        return result;
    }

    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = QStringLiteral("Could not open %1 for writing: %2")
                           .arg(filename, file.errorString());
        FLOG_ERROR(Ch::IO, "FmlArchive::save: {}", result.error);
        mz_free(buf);
        return result;
    }
    file.write(static_cast<const char*>(buf), static_cast<qint64>(size));
    mz_free(buf);

    if (!file.commit()) {
        result.error = QStringLiteral("Could not save %1: %2")
                           .arg(filename, file.errorString());
        FLOG_ERROR(Ch::IO, "FmlArchive::save: {}", result.error);
        return result;
    }

    return result;
}

FmlResult FmlArchive::load(const QString& filename,
                           CanvasScene* scene,
                           ThreadedIO* worker)
{
    FmlResult result;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Could not open %1: %2")
                           .arg(filename, file.errorString());
        FLOG_ERROR(Ch::IO, "FmlArchive::load: {}", result.error);
        return result;
    }

    if (!looks_like_zip(file)) {
        return load_legacy(file, scene, worker);
    }

    QByteArray bytes = file.readAll();
    file.close();

    ZipReader zip;
    if (!zip.initMem(bytes.constData(), static_cast<size_t>(bytes.size()))) {
        result.error
            = QStringLiteral("%1 is not a valid zip archive").arg(filename);
        FLOG_ERROR(Ch::IO, "FmlArchive::load: {}", result.error);
        return result;
    }

    int manifestIndex
        = mz_zip_reader_locate_file(zip.get(), "manifest.json", nullptr, 0);
    if (manifestIndex < 0) {
        result.error = QStringLiteral("%1 has no manifest.json").arg(filename);
        FLOG_ERROR(Ch::IO, "FmlArchive::load: {}", result.error);
        return result;
    }

    size_t manifestSize = 0;
    void* manifestBuf = mz_zip_reader_extract_to_heap(zip.get(),
                                                      static_cast<mz_uint>(
                                                          manifestIndex),
                                                      &manifestSize,
                                                      0);
    if (!manifestBuf) {
        result.error = QStringLiteral("Could not read manifest.json from %1")
                           .arg(filename);
        FLOG_ERROR(Ch::IO, "FmlArchive::load: {}", result.error);
        return result;
    }
    QByteArray manifestJson(static_cast<const char*>(manifestBuf),
                            static_cast<int>(manifestSize));
    mz_free(manifestBuf);

    QString parseError;
    std::optional<Manifest> manifest = parse_manifest(manifestJson, parseError);
    if (!manifest) {
        result.error = parseError;
        return result;
    }

    // Written before `finished` is emitted below, so the GUI-thread
    // caller (FileActions::loadFmlIntoCurrentTab(), reading this via
    // CanvasView::restoreCanvasRect() once that signal arrives) always
    // sees this value - Qt's queued cross-thread signal delivery
    // provides the necessary happens-before ordering, so no extra
    // locking is needed for this single write-before-emit.
    scene->setRememberedBoundingRect(manifest->sceneBoundingRect);

    if (worker) {
        emit worker->beginProcessing(manifest->items.size());
    }

    for (int i = 0; i < manifest->items.size(); ++i) {
        if (worker && worker->canceled) {
            break;
        }

        const ManifestItem& mi = manifest->items.at(i);

        QVariantMap itemData;
        itemData[QStringLiteral("type")] = mi.type;
        itemData[QStringLiteral("id")] = mi.id;
        itemData[QStringLiteral("x")] = mi.x;
        itemData[QStringLiteral("y")] = mi.y;
        itemData[QStringLiteral("z")] = mi.z;
        itemData[QStringLiteral("scale")] = mi.scale;
        itemData[QStringLiteral("rotation")] = mi.rotation;
        itemData[QStringLiteral("flip")] = mi.flip;
        itemData[QStringLiteral("data")] = mi.data;

        if (mi.type == QStringLiteral("pixmap")
            || mi.type == QStringLiteral("gif")) {
            QByteArray imagePath = mi.image.toUtf8();
            int imageIndex = mz_zip_reader_locate_file(zip.get(),
                                                       imagePath.constData(),
                                                       nullptr,
                                                       0);

            bool ok = false;
            if (imageIndex >= 0) {
                size_t imgSize = 0;
                void* imgBuf
                    = mz_zip_reader_extract_to_heap(zip.get(),
                                                    static_cast<mz_uint>(
                                                        imageIndex),
                                                    &imgSize,
                                                    0);
                if (imgBuf) {
                    if (mi.type == QStringLiteral("gif")) {
                        // Raw bytes, not decoded to a static QImage -
                        // GifItem's constructor needs the whole animated
                        // stream, not just one frame.
                        itemData[QStringLiteral("gifBytes")]
                            = QByteArray(static_cast<const char*>(imgBuf),
                                         static_cast<int>(imgSize));
                        itemData[QStringLiteral("filename")] = mi.data.value(
                            QStringLiteral("filename"));
                        ok = true;
                    } else {
                        QImage image;
                        ok = image.loadFromData(static_cast<const uchar*>(
                                                    imgBuf),
                                                static_cast<int>(imgSize));
                        if (ok) {
                            itemData[QStringLiteral("image")] = image;
                            itemData[QStringLiteral("filename")]
                                = mi.data.value(QStringLiteral("filename"));
                        }
                    }
                    mz_free(imgBuf);
                }
            }

            if (!ok) {
                result.itemErrors.append(
                    QStringLiteral("Could not load image for item %1")
                        .arg(mi.id.toString(QUuid::WithoutBraces)));
                FLOG_WARN(Ch::IO,
                          "FmlArchive::load: could not load image for item "
                          "{}",
                          mi.id.toString(QUuid::WithoutBraces));
                if (worker) {
                    emit worker->progress(i);
                }
                continue;
            }
        }

        scene->add_item_later(itemData, false);

        if (worker) {
            emit worker->progress(i);
        }
    }

    return result;
}

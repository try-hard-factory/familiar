#include "canvasscene.h"
#include "core/settings.h"
#include "fileio.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>

#include <algorithm>

// An import decodes the file into a QImage and keeps only that, so the
// first save encoded the decoded pixels all over again - a JPEG import
// stored as JPEG was re-compressed before the user had touched
// anything, and the periodic autosave alone was enough to do it. The
// bytes are right there in ImageImportSession::run(), and they are
// exactly what the pixmap was decoded from, so they can be stored
// verbatim instead; PixmapItem::adopt_import_source() is where that is
// decided and these tests are about that decision.
//
// They do NOT go through FmlArchive: pixmap_to_bytes() is what the
// archive asks each picture for, and what it does with the answer is
// its own business (tests/io/fml_* cover that side).

namespace {

// Real noise, not a gradient: a JPEG re-encode of a smooth image is
// nearly lossless, which would leave the generation-loss test below
// unable to tell a re-encode from a verbatim copy.
QImage noise_image(int width, int height)
{
    QImage img(width, height, QImage::Format_RGB32);
    quint32 state = 0x2545F491;
    for (int y = 0; y < height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < width; ++x) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            line[x] = qRgb(state & 0xFF,
                           (state >> 8) & 0xFF,
                           (state >> 16) & 0xFF);
        }
    }
    return img;
}

// Qt's JPEG writer emits no EXIF at all, so the orientation an import
// has to cope with is spliced in by hand: an APP1 segment right after
// SOI, holding a little-endian TIFF block whose IFD0 has the single
// Orientation (0x0112) entry. 6 means "rotate 90 clockwise", which
// QImageReader::setAutoTransform(true) applies while reading - leaving
// the file's own bytes describing a DIFFERENT picture than the decoded
// image, which is the case being pinned.
QByteArray with_exif_orientation(const QByteArray& jpeg, char orientation)
{
    QByteArray app1;
    app1.append("\xFF\xE1", 2);         // APP1
    app1.append("\x00\x22", 2);         // segment length, 34 including itself
    app1.append("Exif\0\0", 6);
    app1.append("II", 2);               // little-endian
    app1.append("\x2A\x00", 2);
    app1.append("\x08\x00\x00\x00", 4); // IFD0 begins at offset 8
    app1.append("\x01\x00", 2);         // one entry
    app1.append("\x12\x01", 2);         // tag 0x0112, Orientation
    app1.append("\x03\x00", 2);         // type SHORT
    app1.append("\x01\x00\x00\x00", 4); // count 1
    app1.append(orientation);
    app1.append("\x00\x00\x00", 3);     // the value field is 4 bytes wide
    app1.append("\x00\x00\x00\x00", 4); // no further IFD

    QByteArray out = jpeg;
    out.insert(2, app1);
    return out;
}

// Largest per-channel difference between two images of the same size -
// 0 only if every pixel is identical.
int max_channel_diff(const QImage& left, const QImage& right)
{
    if (left.size() != right.size()) {
        return 255;
    }
    const QImage a = left.convertToFormat(QImage::Format_RGB32);
    const QImage b = right.convertToFormat(QImage::Format_RGB32);
    int worst = 0;
    for (int y = 0; y < a.height(); ++y) {
        const auto* lineA = reinterpret_cast<const QRgb*>(a.scanLine(y));
        const auto* lineB = reinterpret_cast<const QRgb*>(b.scanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            worst = std::max({worst,
                              std::abs(qRed(lineA[x]) - qRed(lineB[x])),
                              std::abs(qGreen(lineA[x]) - qGreen(lineB[x])),
                              std::abs(qBlue(lineA[x]) - qBlue(lineB[x]))});
        }
    }
    return worst;
}

class ImportLosslessTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
        ASSERT_TRUE(dir_.isValid());
        savedFormat_ = FamSettings::value_or_default(kformatKey);
        savedOptimize_ = FamSettings::value_or_default(koptimizeKey);
        // "warn" only reports, so nothing is resized unless a test asks
        // for it - whatever the machine running this happens to have set.
        FamSettings::set_value(koptimizeKey, QStringLiteral("warn"));
    }

    void TearDown() override
    {
        FamSettings::set_value(kformatKey, savedFormat_);
        FamSettings::set_value(koptimizeKey, savedOptimize_);
    }

    // Writes `img` in `format` and hands back both the url to import and
    // the bytes that landed on disk, which is what a verbatim store has
    // to reproduce.
    QUrl write_picture(const QImage& img,
                       const char* format,
                       QByteArray& fileBytes)
    {
        const QString path = dir_.filePath(QStringLiteral("imported.")
                                           + QString::fromLatin1(format));
        EXPECT_TRUE(img.save(path, format));
        fileBytes = read_file(path);
        EXPECT_FALSE(fileBytes.isEmpty());
        return QUrl::fromLocalFile(path);
    }

    QUrl write_bytes(const QByteArray& bytes, const QString& name)
    {
        const QString path = dir_.filePath(name);
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        EXPECT_EQ(file.write(bytes), bytes.size());
        return QUrl::fromLocalFile(path);
    }

    static QByteArray read_file(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    // Runs the import to completion on THIS thread (run() is an ordinary
    // call, the worker only exists to emit through - same as
    // tests/io/import_session_settings_test.cpp) and drains the queue it
    // fills, which is the GUI-thread half an import normally reaches
    // through CanvasView::on_insert_images_finished().
    PixmapItem* import_one(const QUrl& url)
    {
        ImageImportSession session({url}, QPointF(0, 0), scene_.get());
        ThreadedIO worker([](ThreadedIO*) {});
        session.run(&worker);

        const QList<IBaseItem*> added = scene_->add_queued_items();
        if (added.size() != 1) {
            return nullptr;
        }
        return dynamic_cast<PixmapItem*>(added.first());
    }

    static constexpr auto kformatKey = "Items/image_storage_format";
    static constexpr auto koptimizeKey = "Items/auto_optimize_imported_images";

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
    QTemporaryDir dir_;
    QVariant savedFormat_;
    QVariant savedOptimize_;
};

} // namespace

// "best" picks JPEG for anything opaque over ksmallImageSide, so this is
// the ordinary case: a photo dropped onto the canvas.
TEST_F(ImportLosslessTest, AJpegImportIsStoredAsTheFileItself)
{
    FamSettings::set_value(kformatKey, QStringLiteral("best"));
    QByteArray fileBytes;
    const QUrl url = write_picture(noise_image(800, 600), "JPG", fileBytes);

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);

    const auto [stored, format] = item->pixmap_to_bytes();
    EXPECT_EQ(format, QStringLiteral("jpg"));
    EXPECT_EQ(stored, fileBytes);
}

// The point of all this: what the import put on the canvas and what the
// save writes are the same pixels, with no encoder in between.
TEST_F(ImportLosslessTest, TheFirstSaveDoesNotRecompressAJpegImport)
{
    FamSettings::set_value(kformatKey, QStringLiteral("best"));
    QByteArray fileBytes;
    const QUrl url = write_picture(noise_image(800, 600), "JPG", fileBytes);

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);

    QImage stored;
    ASSERT_TRUE(stored.loadFromData(item->pixmap_to_bytes().first));

    EXPECT_EQ(max_channel_diff(stored, item->pixmap().toImage()), 0);
}

TEST_F(ImportLosslessTest, APngImportIsStoredAsTheFileItself)
{
    FamSettings::set_value(kformatKey, QStringLiteral("png"));
    QByteArray fileBytes;
    const QUrl url = write_picture(noise_image(800, 600), "PNG", fileBytes);

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);

    const auto [stored, format] = item->pixmap_to_bytes();
    EXPECT_EQ(format, QStringLiteral("png"));
    EXPECT_EQ(stored, fileBytes);
}

// Items/image_storage_format is a size decision as much as a format one,
// and it wins: keeping the source instead would put a PNG in the
// archive of someone who asked for JPEG. Deliberate, so pinned.
TEST_F(ImportLosslessTest, ThePngSourceIsNotKeptWhenJpegIsAsked)
{
    FamSettings::set_value(kformatKey, QStringLiteral("jpg"));
    QByteArray fileBytes;
    const QUrl url = write_picture(noise_image(800, 600), "PNG", fileBytes);

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);

    const auto [stored, format] = item->pixmap_to_bytes();
    EXPECT_EQ(format, QStringLiteral("jpg"));
    EXPECT_NE(stored, fileBytes);
}

// Items/auto_optimize_imported_images == "optimize_large" resizes the
// decoded image, so the file on disk no longer describes it at all.
TEST_F(ImportLosslessTest, ADownscaledImportIsNotStoredAsTheFileItself)
{
    FamSettings::set_value(kformatKey, QStringLiteral("best"));
    FamSettings::set_value(koptimizeKey, QStringLiteral("optimize_large"));
    QByteArray fileBytes;
    const QUrl url = write_picture(noise_image(5000, 600), "JPG", fileBytes);

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);
    // The downscale really happened - otherwise this test would pass
    // for the wrong reason.
    ASSERT_EQ(item->pixmap().width(), klargeImageMaxDimension);

    EXPECT_NE(item->pixmap_to_bytes().first, fileBytes);
}

// An EXIF orientation is applied while reading, so here too the bytes
// describe a different picture than the one on the canvas.
TEST_F(ImportLosslessTest, AnExifRotatedImportIsNotStoredAsTheFileItself)
{
    FamSettings::set_value(kformatKey, QStringLiteral("best"));
    QByteArray plain;
    QBuffer buffer(&plain);
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly));
    ASSERT_TRUE(noise_image(800, 600).save(&buffer, "JPG"));

    const QByteArray fileBytes = with_exif_orientation(plain, 6);
    const QUrl url = write_bytes(fileBytes, QStringLiteral("rotated.jpg"));

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);
    // The rotation really was applied - 800x600 came back as 600x800.
    ASSERT_EQ(item->pixmap().width(), 600);

    EXPECT_NE(item->pixmap_to_bytes().first, fileBytes);
}

// The other source that arrives with its own bytes: an embedded data:
// URI (a download behaves the same way). Decoded by
// QImage::loadFromData(), which applies no EXIF orientation - so the
// payload always describes the image exactly, and reloading the .fml
// decodes it through that very same call.
TEST_F(ImportLosslessTest, ADataUriImportIsStoredAsThePayloadItself)
{
    FamSettings::set_value(kformatKey, QStringLiteral("best"));
    QByteArray payload;
    QBuffer buffer(&payload);
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly));
    ASSERT_TRUE(noise_image(800, 600).save(&buffer, "JPG"));

    const QUrl url(QStringLiteral("data:image/jpeg;base64,")
                   + QString::fromLatin1(payload.toBase64()));

    PixmapItem* item = import_one(url);
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(item->pixmap_to_bytes().first, payload);
}

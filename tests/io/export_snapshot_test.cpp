#include "export.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QTemporaryDir>

// The exporters used to run entirely on a ThreadedIO worker, which meant
// QGraphicsScene::render() and PixmapItem::pixmap_to_bytes() walked LIVE
// items while the GUI thread painted the same ones, GIF timers replaced
// their pixmaps and the autosave/recovery timers encoded them. Three
// caches were written from two threads at once (PixmapItem's mutable
// downscaleCache_ and encodedCache_, plus QGraphicsScene's own BSP index,
// which mutates inside a const items() query).
//
// The fix is a hard split: prepare() reads everything on the GUI thread
// into plain values, export_to() reads nothing else. These tests pin that
// split down by taking the items away between the two steps - if the
// worker half still reached for them, it would read freed memory rather
// than quietly produce the right file.

namespace {

// Returned as a prvalue: a QGraphicsItem is neither copyable nor movable,
// so naming a local in here first would not compile.
PixmapItem make_item(const QColor& fill, int side = 600)
{
    QImage img(side, side, QImage::Format_RGB32);
    img.fill(fill);
    return PixmapItem(img);
}

int files_in(const QString& dir)
{
    return static_cast<int>(
        QDir(dir).entryList(QDir::Files | QDir::NoDotAndDotDot).size());
}

} // namespace

// ─── PixmapItem::export_source() / encode() ────────────────────────────────

TEST(ExportSourceTest, EncodingASourceMatchesEncodingTheItem)
{
    PixmapItem item = make_item(Qt::darkCyan);

    const auto [direct, directFormat] = item.pixmap_to_bytes();
    const auto [viaSource, sourceFormat]
        = PixmapItem::encode(item.export_source());

    EXPECT_EQ(sourceFormat, directFormat);
    EXPECT_EQ(viaSource, direct);
}

// The point of the split: the captured source owns its pixels, so it
// still encodes correctly after the item it came from is gone.
TEST(ExportSourceTest, ASourceOutlivesTheItemItWasTakenFrom)
{
    PixmapItem::ExportSource source;
    QByteArray expected;
    {
        PixmapItem item = make_item(Qt::magenta);
        source = item.export_source();
        expected = item.pixmap_to_bytes().first;
    }

    const auto [bytes, format] = PixmapItem::encode(source);
    EXPECT_FALSE(bytes.isEmpty());
    EXPECT_EQ(bytes, expected);

    QImage decoded;
    ASSERT_TRUE(decoded.loadFromData(bytes));
    EXPECT_EQ(decoded.size(), QSize(600, 600));
}

// A GIF frame tick replaces the item's pixmap. A source captured before
// that must keep describing the frame it was taken from, not the new one.
TEST(ExportSourceTest, ReplacingThePixmapDoesNotChangeASourceAlreadyTaken)
{
    PixmapItem item = make_item(Qt::red, 600);
    const PixmapItem::ExportSource source = item.export_source();

    QImage replacement(600, 600, QImage::Format_RGB32);
    replacement.fill(Qt::blue);
    item.setPixmap(QPixmap::fromImage(replacement));

    QImage decoded;
    ASSERT_TRUE(decoded.loadFromData(PixmapItem::encode(source).first));
    // JPEG is lossy, so compare the hue rather than exact bytes.
    const QColor sampled = decoded.pixelColor(300, 300);
    EXPECT_GT(sampled.red(), sampled.blue());
}

// The stored bytes are reused verbatim when they still match the pixmap -
// this is what keeps a picture from being re-encoded generation after
// generation, and it has to survive the split.
TEST(ExportSourceTest, StoredBytesAreCarriedThroughInsteadOfReEncoded)
{
    PixmapItem item = make_item(Qt::darkYellow);
    const QByteArray stored = item.pixmap_to_bytes().first;

    const PixmapItem::ExportSource source = item.export_source();
    EXPECT_EQ(source.encoded, stored);
    EXPECT_TRUE(source.image.isNull());
    EXPECT_TRUE(source.cacheable);
    EXPECT_EQ(PixmapItem::encode(source).first, stored);
}

// A cropped rendering is a different image from the stored one, so it
// must NOT be fed back into the item's cache.
TEST(ExportSourceTest, ACroppedSourceIsNotCacheable)
{
    PixmapItem item = make_item(Qt::darkGreen);
    item.set_crop(QRectF(0, 0, 100, 100));

    const PixmapItem::ExportSource source
        = item.export_source(/*applyGrayscale=*/true, /*applyCrop=*/true);

    EXPECT_FALSE(source.cacheable);
    EXPECT_TRUE(source.encoded.isEmpty());
    EXPECT_EQ(source.image.size(), QSize(100, 100));
}

// ─── ImagesToDirectoryExporter ─────────────────────────────────────────────

TEST(ImagesExportSnapshotTest, PrepareCapturesOneJobPerPicture)
{
    PixmapItem first = make_item(Qt::red);
    PixmapItem second = make_item(Qt::blue);

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ImagesToDirectoryExporter exporter({&first, &second}, dir.path());
    exporter.prepare();

    ASSERT_EQ(exporter.jobs().size(), 2);
    for (const auto& job : exporter.jobs()) {
        EXPECT_FALSE(job.filename.isEmpty());
        EXPECT_FALSE(job.source.format.isEmpty());
        // The name has to be settled during prepare(), extension
        // included, so the worker never has to ask the item anything.
        EXPECT_TRUE(job.filename.endsWith(job.source.format));
    }
}

// The whole point of the refactor, as a test: the pictures are destroyed
// between prepare() and export_to(), and the export still writes them.
TEST(ImagesExportSnapshotTest, ExportRunsWithoutTheItemsBeingAliveAnyMore)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    auto* first = new PixmapItem(QImage(600, 600, QImage::Format_RGB32));
    auto* second = new PixmapItem(QImage(600, 600, QImage::Format_RGB32));
    ImagesToDirectoryExporter exporter({first, second}, dir.path());
    exporter.prepare();

    const QString firstName = exporter.jobs()[0].filename;
    delete first;
    delete second;

    exporter.export_to(nullptr);

    EXPECT_EQ(files_in(dir.path()), 2);
    QImage written;
    ASSERT_TRUE(written.load(QDir(dir.path()).filePath(firstName)));
    EXPECT_EQ(written.size(), QSize(600, 600));
}

TEST(ImagesExportSnapshotTest, AnExistingFileStopsForInputAndResumes)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    PixmapItem only = make_item(Qt::cyan);
    ImagesToDirectoryExporter exporter({&only}, dir.path());
    exporter.prepare();

    // Occupy the target name so the first pass hits the conflict path.
    const QString target = QDir(dir.path()).filePath(exporter.jobs()[0].filename);
    QFile placeholder(target);
    ASSERT_TRUE(placeholder.open(QIODevice::WriteOnly));
    placeholder.write("not an image");
    placeholder.close();

    // Without a worker there is nobody to ask, so the pass just stops
    // with the placeholder untouched.
    exporter.export_to(nullptr);
    EXPECT_EQ(QFileInfo(target).size(), 12);

    // The caller's answer arrives and the resume overwrites it, still
    // without touching the item.
    exporter.set_handle_existing(QStringLiteral("overwrite_all"));
    exporter.export_to(nullptr);

    QImage written;
    ASSERT_TRUE(written.load(target));
    EXPECT_EQ(written.size(), QSize(600, 600));
}

// ─── SceneToSVGExporter::nodes_to_svg() ────────────────────────────────────

TEST(SvgExportSnapshotTest, NodesRenderWithoutAnySceneOrItem)
{
    SceneToSVGExporter::Node picture;
    {
        PixmapItem item = make_item(Qt::darkRed);
        picture.source = item.export_source(true, true);
    }
    picture.width = 120;
    picture.height = 90;
    picture.pos = QPointF(10, 20);
    picture.anchor = QPointF(10, 20);
    picture.opacity = 0.5;

    SceneToSVGExporter::Node text;
    text.isText = true;
    text.text = QStringLiteral("hello");
    text.style = QStringLiteral("font-size:12pt");
    text.pos = QPointF(1, 2);

    const QString svg
        = SceneToSVGExporter::nodes_to_svg({picture, text}, QSize(400, 300));

    EXPECT_TRUE(svg.contains(QStringLiteral("<svg")));
    EXPECT_TRUE(svg.contains(QStringLiteral("width=\"400\"")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<image")));
    EXPECT_TRUE(svg.contains(QStringLiteral("data:image/")));
    EXPECT_TRUE(svg.contains(QStringLiteral("base64,")));
    EXPECT_TRUE(svg.contains(QStringLiteral("opacity=\"0.5\"")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<text")));
    EXPECT_TRUE(svg.contains(QStringLiteral("hello")));
}

TEST(SvgExportSnapshotTest, AFlippedNodeKeepsItsThreePartTransform)
{
    SceneToSVGExporter::Node node;
    node.isText = true;
    node.text = QStringLiteral("x");
    node.flip = -1;
    node.anchor = QPointF(5, 7);
    node.rotation = 30;

    const QString svg
        = SceneToSVGExporter::nodes_to_svg({node}, QSize(100, 100));

    EXPECT_TRUE(svg.contains(QStringLiteral("translate(5 7)")));
    EXPECT_TRUE(svg.contains(QStringLiteral("scale(-1 1)")));
    EXPECT_TRUE(svg.contains(QStringLiteral("translate(-5 -7)")));
    EXPECT_TRUE(svg.contains(QStringLiteral("rotate(30 5 7)")));
}

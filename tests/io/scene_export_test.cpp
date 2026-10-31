#include "canvasscene.h"
#include "export.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QUndoStack>

// Companion to export_snapshot_test.cpp, which pins the worker half down
// with no scene at all. These go through a real CanvasScene instead, so
// they also cover the GUI-thread half: what prepare() actually captures,
// and that the user's selection comes back afterwards (it used to be
// cleared in SceneExporterBase's constructor and never restored).

namespace {

// A MainWindow is ~100ms to build, so one fixture rather than a free
// helper per test.
class SceneExportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
    }

    PixmapItem* add_picture(const QColor& fill, const QPointF& pos)
    {
        QImage img(200, 150, QImage::Format_RGB32);
        img.fill(fill);
        auto* item = new PixmapItem(img);
        item->setPos(pos);
        scene_->addItem(item);
        return item;
    }

    // A real two-frame animated GIF, 4x4, black then white.
    static QByteArray gif_bytes()
    {
        return QByteArray::fromBase64(
            "R0lGODlhBAAEAPAAAAAAAP///yH/C05FVFNDQVBFMi4wAwEAAAAh+QQA"
            "CgAAACwAAAAABAAEAAACBwQAAAAAACgAIfkEAAoAAAAsAAAAAAQABAAA"
            "AgdMkiRJkiQpADs=");
    }

    GifItem* add_gif(const QPointF& pos)
    {
        auto* item = new GifItem(gif_bytes(), QStringLiteral("anim.gif"));
        item->setPos(pos);
        scene_->addItem(item);
        return item;
    }

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
};

} // namespace

// ─── Selection ─────────────────────────────────────────────────────────────

TEST_F(SceneExportTest, TheSelectionIsPutBackWhenTheExporterGoesAway)
{
    PixmapItem* first = add_picture(Qt::red, QPointF(0, 0));
    PixmapItem* second = add_picture(Qt::blue, QPointF(300, 0));
    first->setSelected(true);
    second->setSelected(true);
    ASSERT_EQ(scene_->selectedItems(true).size(), 2);

    {
        SceneToSVGExporter exporter(scene_.get());
        // Taken down for the duration so handles never reach the file.
        EXPECT_TRUE(scene_->selectedItems(true).isEmpty());
    }

    EXPECT_EQ(scene_->selectedItems(true).size(), 2);
    EXPECT_TRUE(first->isSelected());
    EXPECT_TRUE(second->isSelected());
}

TEST_F(SceneExportTest, AnUnselectedSceneStaysUnselected)
{
    add_picture(Qt::red, QPointF(0, 0));

    {
        SceneToSVGExporter exporter(scene_.get());
    }

    EXPECT_TRUE(scene_->selectedItems(true).isEmpty());
}

// Restoring goes through find_by_uid(), so an item that disappeared
// while the export ran is skipped rather than dereferenced.
TEST_F(SceneExportTest, AnItemRemovedDuringTheExportIsNotReselected)
{
    PixmapItem* staying = add_picture(Qt::red, QPointF(0, 0));
    PixmapItem* leaving = add_picture(Qt::blue, QPointF(300, 0));
    staying->setSelected(true);
    leaving->setSelected(true);

    {
        SceneToSVGExporter exporter(scene_.get());
        // removeItem() releases the scene's shared_ptr, which is the
        // only owner - the item is gone when this returns.
        scene_->removeItem(leaving);
    }

    EXPECT_EQ(scene_->selectedItems(true).size(), 1);
    EXPECT_TRUE(staying->isSelected());
}

// ─── SceneToSVGExporter ────────────────────────────────────────────────────

TEST_F(SceneExportTest, SvgPrepareCapturesOneNodePerItem)
{
    add_picture(Qt::red, QPointF(0, 0));
    add_picture(Qt::blue, QPointF(300, 0));

    SceneToSVGExporter exporter(scene_.get());
    ASSERT_TRUE(exporter.get_user_input(nullptr));
    exporter.prepare();

    ASSERT_EQ(exporter.nodes().size(), 2);
    for (const auto& node : exporter.nodes()) {
        EXPECT_FALSE(node.isText);
        EXPECT_DOUBLE_EQ(node.width, 200.0);
        EXPECT_DOUBLE_EQ(node.height, 150.0);
        EXPECT_FALSE(node.source.format.isEmpty());
    }
}

// The split, end to end: everything the scene contributes is captured by
// prepare(), so the writing half still works once the items are gone.
TEST_F(SceneExportTest, SvgRendersAfterTheItemsAreDestroyed)
{
    PixmapItem* first = add_picture(Qt::red, QPointF(0, 0));
    PixmapItem* second = add_picture(Qt::blue, QPointF(300, 0));

    SceneToSVGExporter exporter(scene_.get());
    ASSERT_TRUE(exporter.get_user_input(nullptr));
    exporter.prepare();
    const QList<SceneToSVGExporter::Node> nodes = exporter.nodes();

    scene_->removeItem(first);
    scene_->removeItem(second);

    const QString svg
        = SceneToSVGExporter::nodes_to_svg(nodes, QSize(800, 600));

    EXPECT_EQ(svg.count(QStringLiteral("<image")), 2);
    EXPECT_TRUE(svg.contains(QStringLiteral("base64,")));
}

// ─── SceneToPixmapExporter ─────────────────────────────────────────────────

TEST_F(SceneExportTest, PixmapExportRendersBeforeTheWorkerHalfRuns)
{
    PixmapItem* only = add_picture(Qt::red, QPointF(0, 0));

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString target = QDir(dir.path()).filePath(QStringLiteral("out.png"));

    SceneToPixmapExporter exporter(scene_.get());
    exporter.set_size(QSize(320, 240));
    exporter.prepare();

    // Nothing of the scene is left by the time the file gets written.
    scene_->removeItem(only);

    exporter.export_to(target, nullptr);

    QImage written;
    ASSERT_TRUE(written.load(target));
    EXPECT_EQ(written.size(), QSize(320, 240));
}

// ─── Picture collection ────────────────────────────────────────────────────

// Images -> Export Images collected its pictures with
// items_by_type("pixmap"), an exact string match. GifItem derives from
// PixmapItem but reports "gif", so every animated picture was silently
// left out and no gif ever reached the export folder next to the stills.
TEST_F(SceneExportTest, EveryPictureIsCollectedAnimatedOnesIncluded)
{
    add_picture(Qt::red, QPointF(0, 0));
    add_gif(QPointF(300, 0));
    add_picture(Qt::blue, QPointF(600, 0));

    EXPECT_EQ(scene_->pictures().size(), 3);
    EXPECT_EQ(scene_->items_by_type("pixmap").size(), 2);
    EXPECT_EQ(scene_->items_by_type("gif").size(), 1);
}

// Notes and groups are not pictures and must not join the export.
TEST_F(SceneExportTest, OnlyPicturesAreCollected)
{
    add_picture(Qt::red, QPointF(0, 0));
    add_gif(QPointF(300, 0));
    auto* note = new TextItem();
    note->setPlainText(QStringLiteral("note"));
    scene_->addItem(note);

    const QList<PixmapItem*> pictures = scene_->pictures();
    ASSERT_EQ(pictures.size(), 2);
    for (PixmapItem* picture : pictures) {
        EXPECT_NE(dynamic_cast<QGraphicsItem*>(picture),
                  static_cast<QGraphicsItem*>(note));
    }
}

// A gif has to come out as the original animation, not one still frame -
// which is what encoding it through the pixmap path would produce.
TEST_F(SceneExportTest, AnAnimatedGifIsExportedVerbatim)
{
    add_gif(QPointF(0, 0));

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ImagesToDirectoryExporter exporter(scene_->pictures(), dir.path());
    exporter.prepare();

    ASSERT_EQ(exporter.jobs().size(), 1);
    EXPECT_TRUE(exporter.jobs()[0].filename.endsWith(QStringLiteral(".gif")));
    EXPECT_EQ(exporter.jobs()[0].gifBytes, gif_bytes());

    exporter.export_to(nullptr);

    QFile written(QDir(dir.path()).filePath(exporter.jobs()[0].filename));
    ASSERT_TRUE(written.open(QIODevice::ReadOnly));
    EXPECT_EQ(written.readAll(), gif_bytes());
}


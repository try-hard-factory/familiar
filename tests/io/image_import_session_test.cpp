#include "canvasscene.h"
#include "core/settings.h"
#include "fileio.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <gtest/gtest.h>

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>

namespace {

class ImageImportSessionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
        ASSERT_TRUE(dir_.isValid());
        oldLimit_ = FamSettings::value_or_default(kLimitSetting);
        oldRawChoice_ = FamSettings::value_or_default(kRawSetting);
    }

    void TearDown() override
    {
        FamSettings::set_value(kLimitSetting, oldLimit_);
        FamSettings::set_value(kRawSetting, oldRawChoice_);
    }

    QString write_png(const QString& name, const QSize& size = {2, 2})
    {
        const QString path = dir_.filePath(name);
        QImage image(size, QImage::Format_RGB32);
        image.fill(Qt::red);
        EXPECT_TRUE(image.save(path, "PNG"));
        return path;
    }

    static constexpr auto kLimitSetting = "Items/image_allocation_limit";
    static constexpr auto kRawSetting = "Items/raw_import_choice";

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
    QTemporaryDir dir_;
    QVariant oldLimit_;
    QVariant oldRawChoice_;
};

TEST(ImageImportFileTypeTest, RawExtensionMatchingIsCaseInsensitive)
{
    EXPECT_TRUE(is_raw_file(QStringLiteral("portrait.NEF")));
    EXPECT_TRUE(is_raw_file(QStringLiteral("portrait.cr3")));
    EXPECT_TRUE(is_raw_file(QStringLiteral("portrait.DnG")));
    EXPECT_FALSE(is_raw_file(QStringLiteral("portrait.png")));
    EXPECT_FALSE(is_raw_file(QStringLiteral("portrait.nef.backup")));
}

TEST_F(ImageImportSessionTest, ImportsAnEmbeddedDataUrlWithoutNetwork)
{
    QImage image(4, 2, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly));
    ASSERT_TRUE(image.save(&buffer, "PNG"));
    const QUrl url = QUrl::fromEncoded(QByteArray("data:image/png;base64,")
                                      + png.toBase64());
    ImageImportSession session({url}, QPointF(20, 10), scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    QStringList errors;
    QObject::connect(&worker, &ThreadedIO::work_finished,
                     [&errors](const QString&, const QStringList& found) {
                         errors = found;
                     });

    session.run(&worker);
    const QList<IBaseItem*> added = scene_->add_queued_items();

    EXPECT_TRUE(errors.isEmpty());
    ASSERT_EQ(added.size(), 1);
    auto* picture = dynamic_cast<PixmapItem*>(added.first());
    ASSERT_NE(picture, nullptr);
    EXPECT_EQ(picture->pixmap().size(), QSize(4, 2));
    EXPECT_EQ(picture->pos(), QPointF(18, 9));
}

TEST_F(ImageImportSessionTest, ImportsLocalPngAndKeepsItsSourceFilename)
{
    const QString path = write_png(QStringLiteral("board.png"), {6, 4});
    ImageImportSession session({QUrl::fromLocalFile(path)}, QPointF(10, 10),
                               scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    QString currentItem;
    QObject::connect(&worker, &ThreadedIO::current_item_changed,
                     [&currentItem](const QString& label) { currentItem = label; });

    session.run(&worker);
    const QList<IBaseItem*> added = scene_->add_queued_items();

    EXPECT_EQ(currentItem, QStringLiteral("board.png"));
    ASSERT_EQ(added.size(), 1);
    auto* picture = dynamic_cast<PixmapItem*>(added.first());
    ASSERT_NE(picture, nullptr);
    EXPECT_EQ(picture->pixmap().size(), QSize(6, 4));
    EXPECT_EQ(picture->pos(), QPointF(7, 8));
}

TEST_F(ImageImportSessionTest, ValidImagesStillImportWhenAnotherFileFails)
{
    const QString good = write_png(QStringLiteral("good.png"));
    const QString bad = dir_.filePath(QStringLiteral("bad.invalid"));
    QFile file(bad);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("not an image"), 12);
    file.close();
    ImageImportSession session({QUrl::fromLocalFile(bad), QUrl::fromLocalFile(good)},
                               {}, scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    QStringList errors;
    QObject::connect(&worker, &ThreadedIO::work_finished,
                     [&errors](const QString&, const QStringList& found) {
                         errors = found;
                     });

    session.run(&worker);

    EXPECT_EQ(errors, QStringList{bad});
    EXPECT_EQ(scene_->add_queued_items().size(), 1);
}

TEST_F(ImageImportSessionTest, UnsupportedImageIsReportedWithoutAddingAnItem)
{
    const QString path = dir_.filePath(QStringLiteral("unknown.format"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("not an image"), 12);
    file.close();
    ImageImportSession session({QUrl::fromLocalFile(path)}, {}, scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    QStringList unsupported;
    QStringList finishedErrors;
    QObject::connect(&worker, &ThreadedIO::image_load_failures,
                     [&unsupported](const QStringList& formats,
                                    const QStringList&, const QStringList&) {
                         unsupported = formats;
                     });
    QObject::connect(&worker, &ThreadedIO::work_finished,
                     [&finishedErrors](const QString&, const QStringList& found) {
                         finishedErrors = found;
                     });

    session.run(&worker);

    EXPECT_EQ(unsupported, QStringList{path});
    EXPECT_EQ(finishedErrors, QStringList{path});
    EXPECT_TRUE(scene_->add_queued_items().isEmpty());
}

TEST_F(ImageImportSessionTest, ImageAboveAllocationLimitIsReportedSeparately)
{
    const QString path = write_png(QStringLiteral("large.png"), {1024, 1024});
    FamSettings::set_value(kLimitSetting, 1); // 1 MiB vs at least 4 MiB decoded.
    ImageImportSession session({QUrl::fromLocalFile(path)}, {}, scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    QStringList tooLarge;
    QObject::connect(&worker, &ThreadedIO::image_load_failures,
                     [&tooLarge](const QStringList&, const QStringList& large,
                                 const QStringList&) { tooLarge = large; });

    session.run(&worker);

    EXPECT_EQ(tooLarge, QStringList{path});
    EXPECT_TRUE(scene_->add_queued_items().isEmpty());
}

TEST_F(ImageImportSessionTest, CancellationDoesNotQueueTheDecodedImage)
{
    const QString path = write_png(QStringLiteral("cancel.png"));
    ImageImportSession session({QUrl::fromLocalFile(path)}, {}, scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    int failureReports = 0;
    QObject::connect(&worker, &ThreadedIO::image_load_failures,
                     [&failureReports](const QStringList&, const QStringList&,
                                       const QStringList&) { ++failureReports; });
    worker.on_canceled();

    session.run(&worker);

    EXPECT_EQ(failureReports, 0);
    EXPECT_TRUE(scene_->add_queued_items().isEmpty());
}

TEST_F(ImageImportSessionTest, RawFilePausesBeforeDecodingUntilChoiceIsMade)
{
    const QString path = dir_.filePath(QStringLiteral("camera.NEF"));
    FamSettings::set_value(kRawSetting, QStringLiteral("ask"));
    ImageImportSession session({QUrl::fromLocalFile(path)}, {}, scene_.get());
    ThreadedIO worker([](ThreadedIO*) {});
    QString requested;
    int finished = 0;
    QObject::connect(&worker, &ThreadedIO::raw_import_choice_required,
                     [&requested](const QString& file) { requested = file; });
    QObject::connect(&worker, &ThreadedIO::work_finished,
                     [&finished](const QString&, const QStringList&) { ++finished; });

    session.run(&worker);

    EXPECT_EQ(requested, path);
    EXPECT_EQ(session.pending_raw_file(), path);
    EXPECT_EQ(finished, 0);
    EXPECT_TRUE(scene_->add_queued_items().isEmpty());
}

} // namespace

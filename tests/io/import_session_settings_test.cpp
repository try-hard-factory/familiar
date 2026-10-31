#include "canvasscene.h"
#include "core/settings.h"
#include "fileio.h"
#include "mainwindow.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>

// ImageImportSession::run() used to call SettingsHandler/FamSettings
// directly, from the ThreadedIO worker. SettingsHandler keeps its whole
// store in a bare QJsonObject with no synchronisation of any kind
// (core/settingshandler.h: json_value() reads document_, set_json_value()
// inserts into it), so that read raced any write the GUI made. Nothing
// could reach such a write - an import runs behind a WindowModal
// ProgressDialog and the settings window is ApplicationModal on top of
// that - but it was the last Settings read left off the GUI thread, and
// it held only by the modality of a dialog.
//
// reread_settings() takes the values on the GUI thread now, at
// construction and again before every resume. These tests pin both
// halves: a run goes by what was taken, and a resume does pick up a
// change (which is what "remember my choice" in RawImportDialog needs).
//
// Items/auto_optimize_imported_images is the observable used here
// because it is decided entirely inside run() from the snapshot.
// Items/image_allocation_limit would not work: it only CLASSIFIES a load
// that already failed, and what makes one fail is Qt's own global
// QImageReader::setAllocationLimit(), set from a post-save callback on
// the GUI thread - never from the worker, so never part of this problem.

namespace {

class ImportSessionSettingsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scene_ = std::make_unique<CanvasScene>(window_, zCounter_, &undoStack_);
        ASSERT_TRUE(dir_.isValid());
        // Restored in TearDown so the rest of the suite is unaffected.
        savedMode_ = FamSettings::value_or_default(kmodeKey);
    }

    void TearDown() override { FamSettings::set_value(kmodeKey, savedMode_); }

    // Long side over klargeImageMaxDimension (4096), but only ~2MB
    // decoded, so no allocation limit is anywhere near it.
    QUrl large_picture()
    {
        const QString path = dir_.filePath(QStringLiteral("wide.png"));
        QImage img(5000, 100, QImage::Format_RGB32);
        img.fill(Qt::darkCyan);
        EXPECT_TRUE(img.save(path));
        return QUrl::fromLocalFile(path);
    }

    // Runs the session to completion on THIS thread - run() is an
    // ordinary function call, the worker is only there to emit through.
    static QStringList large_images_from(ImageImportSession& session)
    {
        QStringList large;
        ThreadedIO worker([](ThreadedIO*) {});
        QObject::connect(&worker,
                         &ThreadedIO::large_images_found,
                         [&large](const QStringList& found) { large = found; });
        session.run(&worker);
        return large;
    }

    static constexpr auto kmodeKey = "Items/auto_optimize_imported_images";

    MainWindow window_;
    uint64_t zCounter_ = 0;
    QUndoStack undoStack_;
    std::unique_ptr<CanvasScene> scene_;
    QTemporaryDir dir_;
    QVariant savedMode_;
};

} // namespace

TEST_F(ImportSessionSettingsTest, TheModeInForceAtConstructionIsTheOneApplied)
{
    FamSettings::set_value(kmodeKey, QStringLiteral("warn"));
    ImageImportSession session({large_picture()}, QPointF(0, 0), scene_.get());

    // The user changes their mind while the import is already running.
    // run() must not notice: it reads no settings at all.
    FamSettings::set_value(kmodeKey, QStringLiteral("optimize_large"));

    EXPECT_EQ(large_images_from(session).size(), 1);
}

TEST_F(ImportSessionSettingsTest, ASessionBuiltAfterTheChangeUsesTheNewMode)
{
    FamSettings::set_value(kmodeKey, QStringLiteral("optimize_large"));
    ImageImportSession session({large_picture()}, QPointF(0, 0), scene_.get());

    // Downscaled instead of reported.
    EXPECT_TRUE(large_images_from(session).isEmpty());
}

// What "remember my choice" in RawImportDialog relies on: the value is
// written by the GUI while the worker is stopped, and the resume has to
// run by it.
TEST_F(ImportSessionSettingsTest, RereadingPicksUpAChangeMadeSinceConstruction)
{
    FamSettings::set_value(kmodeKey, QStringLiteral("optimize_large"));
    ImageImportSession session({large_picture()}, QPointF(0, 0), scene_.get());

    FamSettings::set_value(kmodeKey, QStringLiteral("warn"));
    session.reread_settings();

    EXPECT_EQ(large_images_from(session).size(), 1);
}

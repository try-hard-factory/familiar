#include "canvasview.h"
#include "fileio.h"
#include "mainwindow.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QImage>
#include <QList>
#include <QPoint>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <atomic>

// ~CanvasView used to destroy the view - and scene_ with it - without
// cancelling or waiting for any worker still running. Every worker
// function body captures `this` and reaches members through it
// (imageImportSession_, imagesExporter_, sceneExporter_), and the import
// session additionally holds scene_, so a thread still inside one of
// those bodies read freed memory. The workers also have no parent, so
// nothing destroyed them either: they simply kept running.
//
// It was reachable, which is what made it worth fixing ahead of the
// other threading findings: ProgressDialog is WindowModal only while
// visible, and QDialog::reject() - what Cancel and Esc do - hides it at
// once, while cancellation is only a flag the worker notices at its next
// poll. In between, the main window is live and the tab can be closed.

namespace {

class WorkerLifetimeTest : public ::testing::Test
{
protected:
    // Enough separate files that the import is still in progress when
    // the view goes away a moment later.
    QList<QUrl> many_pictures(int count)
    {
        EXPECT_TRUE(dir_.isValid());
        QList<QUrl> urls;
        QImage img(1200, 900, QImage::Format_RGB32);
        img.fill(Qt::darkCyan);
        for (int i = 0; i < count; ++i) {
            const QString path
                = dir_.filePath(QStringLiteral("pic%1.png").arg(i));
            EXPECT_TRUE(img.save(path));
            urls.append(QUrl::fromLocalFile(path));
        }
        return urls;
    }

    MainWindow window_;
    QTemporaryDir dir_;
};

} // namespace

// ─── ThreadedIO, the mechanism the destructor relies on ────────────────────

TEST(ThreadedIoTest, CancelAndWaitStopTheWorkerBeforeItIsDeleted)
{
    std::atomic<bool> sawCancel{false};
    auto* worker = new ThreadedIO([&sawCancel](ThreadedIO* w) {
        while (!w->is_canceled()) {
            ThreadedIO::sleep_ms(1);
        }
        sawCancel = true;
    });

    worker->start();
    while (!worker->isRunning()) {
        ThreadedIO::sleep_ms(1);
    }

    worker->on_canceled();
    EXPECT_TRUE(worker->wait());

    EXPECT_TRUE(sawCancel.load());
    EXPECT_TRUE(worker->isFinished());
    // Deleting a thread that is still Running is qFatal in Qt; after
    // wait() it is not.
    delete worker;
}

// work_finished() is deliberately NOT named finished(): QThread has its
// own finished() and a same-named signal here would hide it. Both must
// exist, and separately.
TEST(ThreadedIoTest, TheWorkSignalAndTheThreadSignalAreDistinct)
{
    int workFinished = 0;
    int threadFinished = 0;

    auto* worker = new ThreadedIO([](ThreadedIO* w) {
        emit w->work_finished(QString(), {});
    });
    QObject::connect(worker,
                     &ThreadedIO::work_finished,
                     [&workFinished](const QString&, const QStringList&) {
                         ++workFinished;
                     });
    QObject::connect(worker, &QThread::finished, [&threadFinished] {
        ++threadFinished;
    });

    worker->start();
    ASSERT_TRUE(worker->wait());
    // QThread::finished is queued to this thread; let it through.
    QCoreApplication::processEvents();

    EXPECT_EQ(workFinished, 1);
    EXPECT_EQ(threadFinished, 1);
    delete worker;
}

// ─── The destructor itself ─────────────────────────────────────────────────

// The regression: destroying the view while an import is mid-flight.
// Before the fix the worker kept running against a deleted CanvasScene.
TEST_F(WorkerLifetimeTest, DestroyingTheViewStopsAnImportThatIsStillRunning)
{
    auto view = std::make_unique<CanvasView>(window_);
    view->do_insert_images(many_pictures(40), QPoint(0, 0));

    // No wait, no cancel, no dialog dismissed - exactly what closing the
    // tab right after hitting Esc does.
    view.reset();

    // Getting here at all is the assertion: the destructor cancelled and
    // waited, so nothing is left reading the scene it just deleted.
    SUCCEED();
}

// The same for the images export, whose worker is deleted explicitly
// rather than on QThread::finished.
TEST_F(WorkerLifetimeTest, DestroyingTheViewIsSafeWithNoWorkerRunning)
{
    auto view = std::make_unique<CanvasView>(window_);
    view.reset();
    SUCCEED();
}

#ifndef FILEIO_H
#define FILEIO_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <functional>

class CanvasScene;

// Dedicated thread for loading and saving.
class ThreadedIO : public QThread
{
    Q_OBJECT

public:
    using WorkerFunc = std::function<void(ThreadedIO*)>;

    explicit ThreadedIO(WorkerFunc func, QObject* parent = nullptr);

    std::atomic<bool> canceled{false};

    // QThread::msleep() is protected; expose it for worker functions
    // (which are not members of this class) the same way Python's
    // worker.msleep(10) is just called on the thread object directly.
    void sleepMs(unsigned long ms) { QThread::msleep(ms); }

protected:
    void run() override;

signals:
    void progress(int value);
    void finished(const QString& error, const QStringList& errors);
    void beginProcessing(int count);
    void userInputRequired(const QString& message);

public slots:
    void onCanceled();

private:
    WorkerFunc func_;
};

// Adds images to an existing scene. Intended to run as a ThreadedIO
// worker function (see ThreadedIO above); reports progress/completion
// through `worker`'s signals and can be interrupted via `worker->canceled`.
//
// Each url is either a local file (QUrl::isLocalFile()), an embedded
// "data:" URI (decoded in-place, no network), or an http(s) URL
// (downloaded synchronously via QNetworkAccessManager + a nested
// QEventLoop - this runs on ThreadedIO's background thread, which has no
// event loop of its own to deliver QNetworkReply's signals otherwise).
// Queues the decoded QImage via CanvasScene::add_item_later()
// (mutex-protected, safe to call from any thread) — it never touches the
// QGraphicsScene itself. The caller is expected to drain the queue with
// CanvasScene::add_queued_items() on the GUI thread, e.g. from a slot
// connected to ThreadedIO::progress/finished (see CanvasView::do_insert_images).
void load_images(const QList<QUrl>& urls,
                  const QPointF& pos,
                  CanvasScene* scene,
                  ThreadedIO* worker);

// Load familiar's native project file.
//
// TODOLATER: SQLiteIO-based read not ported (archive/manifest format not
// designed yet). Signature kept as in Python's load_bee(filename, scene,
// worker=None).
void load_fml(const QString& filename,
              CanvasScene* scene,
              ThreadedIO* worker = nullptr);

// Save familiar's native project file.
//
// TODOLATER: SQLiteIO-based write not ported (archive/manifest format not
// designed yet). Signature kept as in Python's
// save_bee(filename, scene, create_new=False, worker=None).
void save_fml(const QString& filename,
              CanvasScene* scene,
              bool createNew = false,
              ThreadedIO* worker = nullptr);

#endif // FILEIO_H

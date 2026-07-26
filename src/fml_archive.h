#ifndef FML_ARCHIVE_H
#define FML_ARCHIVE_H

// Reads/writes familiar's .fml project file format: a zip container with
// manifest.json + images/<uid>.<ext>. See docs/fml_format_design.md for
// the full format spec and the rationale behind it.

#include <QRectF>
#include <QString>
#include <QStringList>

class CanvasScene;
class ThreadedIO;

// Result of a save/load operation.
//   error       - fatal error (file unreadable, corrupt manifest, ...);
//                 empty means the operation otherwise succeeded.
//   itemErrors  - non-fatal per-item problems (e.g. one image in the
//                 archive failed to decode); the rest of the file still
//                 loaded/saved normally.
struct FmlResult
{
    QString error;
    QStringList itemErrors;
};

class FmlArchive
{
public:
    // Serializes every item in `scene` (see CanvasScene::items_for_save())
    // into a zip archive and atomically replaces `filename` with it.
    // `canvasRect` is the view's remembered empty-space extent
    // (CanvasView::canvasRect()) - passed in explicitly rather than
    // stashed on CanvasScene, since CanvasView is the only thing that
    // keeps it continuously up to date and nothing else should be
    // writing to CanvasScene::rememberedBoundingRect() while a load might
    // be using it as a one-shot transport slot (see load() below).
    // Synchronous; `worker`, if given, is only used to report progress
    // (beginProcessing/progress) and to check for cancellation - callers
    // that want this off the GUI thread wrap the call in a ThreadedIO
    // themselves (see save_fml() in fileio.cpp).
    static FmlResult save(CanvasScene* scene,
                          const QRectF& canvasRect,
                          const QString& filename,
                          ThreadedIO* worker = nullptr);

    // Reads `filename` (a .fml zip, or a legacy pre-zip .fml file - see
    // docs/fml_format_design.md §10) and queues its items via
    // scene->add_item_later(...). Does NOT touch the QGraphicsScene
    // itself, so this is safe to call from a background thread; the
    // caller must drain the queue with scene->add_queued_items() on the
    // GUI thread once this returns (see load_fml() in fileio.cpp).
    static FmlResult load(const QString& filename,
                          CanvasScene* scene,
                          ThreadedIO* worker = nullptr);
};

#endif // FML_ARCHIVE_H

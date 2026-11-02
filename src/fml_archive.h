#ifndef FML_ARCHIVE_H
#define FML_ARCHIVE_H

// Reads/writes familiar's .fml project file format: a zip container with
// manifest.json + images/<uid>.<ext>. See docs/fml_format_design.md for
// the full format spec and the rationale behind it.

#include <QByteArray>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

class CanvasScene;
class QJsonObject;
class ThreadedIO;

// Result of a save/load operation.
//   error       - fatal error (file unreadable, corrupt manifest, ...);
//                 empty means the operation otherwise succeeded.
//   itemErrors  - items that did NOT make it: one image in the archive
//                 failed to decode, one could not be stored on save.
//                 That content is gone; the rest of the file still
//                 loaded/saved normally.
//   itemRepairs - items that DID make it, with something about them
//                 corrected on the way in (a cycle in the group graph,
//                 a scale of zero, a crop outside the image). Kept
//                 apart from itemErrors on purpose: counting the two
//                 together told the user that pictures sitting right in
//                 front of them had failed to load.
struct FmlResult
{
    QString error;
    QStringList itemErrors;
    QStringList itemRepairs;
};

// Everything a save needs from the scene, captured as plain values -
// the same split export.h describes for the image/SVG exporters, and
// for the same reason: capture() reads the items, write() reads nothing
// but this, so the second half may run while the user keeps editing.
//
// The image bytes are refcounted copies of what the items already hold
// (PixmapItem::encodedCache_), so holding them costs nothing until an
// item's pixmap is actually replaced underneath.
struct FmlSnapshot
{
    struct Image
    {
        QString archiveName; // "images/<uid>.<ext>"
        // Carried alongside only so write(), which has no items to look
        // anything up in, can name the item in FmlResult::itemErrors
        // exactly as the one-pass version used to.
        QString itemId;
        QByteArray bytes; // already encoded
    };

    // Built by capture() too: the manifest describes the items as they
    // were read, so it cannot be assembled later from anything else.
    QByteArray manifestJson;
    QList<Image> images;
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
    // writing to CanvasScene::remembered_bounding_rect() while a load might
    // be using it as a one-shot transport slot (see load() below).
    // Synchronous; `worker`, if given, is only used to report progress
    // (beginProcessing/progress) and to check for cancellation - callers
    // that want this off the GUI thread wrap the call in a ThreadedIO
    // themselves (see save_fml() in fileio.cpp).
    //
    // Exactly capture() followed by write(); a caller that needs the two
    // halves apart calls them itself.
    static FmlResult save(CanvasScene* scene,
                          const QRectF& canvasRect,
                          const QString& filename,
                          ThreadedIO* worker = nullptr);

    // GUI thread only: this is the only half that reads `scene`, its
    // items or their QPixmaps, and write() reads none of them. Returns
    // false only when `worker` reported a cancellation, in which case
    // the caller writes nothing and the file already on disk (if any)
    // stays as it is.
    //
    // `worker` gets the progress reports, because this is the half whose
    // per-item cost dominates a FIRST save: encoding the images. On
    // every save after it they come out of PixmapItem's encode cache
    // and nearly all the cost moves to write().
    static bool capture(CanvasScene* scene,
                        const QRectF& canvasRect,
                        FmlSnapshot& snapshot,
                        ThreadedIO* worker = nullptr);

    // Assembles the zip from `snapshot` and atomically replaces
    // `filename` with it. Touches no scene, no item and no QPixmap, so
    // this is the half that may run on a worker thread with the GUI
    // still live - see familiar::recovery::save() (recovery.h), which
    // does exactly that.
    static FmlResult write(const FmlSnapshot& snapshot,
                           const QString& filename);

    // Reads `filename` (a .fml zip, or a legacy pre-zip .fml file - see
    // docs/fml_format_design.md §10) and queues its items via
    // scene->add_item_later(...). Does NOT touch the QGraphicsScene
    // itself, so this is safe to call from a background thread; the
    // caller must drain the queue with scene->add_queued_items() on the
    // GUI thread once this returns (see load_fml() in fileio.cpp).
    static FmlResult load(const QString& filename,
                          CanvasScene* scene,
                          ThreadedIO* worker = nullptr);

    // Drops group memberships and attachments that a hand-edited or
    // corrupted manifest could have made cyclic, in place, and returns
    // one message per drop (which load() reports as itemRepairs - the
    // items themselves are kept, only their links go). A cycle
    // there is not a cosmetic problem: GroupItem's ancestor walks never
    // terminate on one - see the definition's own comment.
    //
    // Called by load() itself; public only so a test can hand it a
    // crafted manifest directly, since building the equivalent .fml plus
    // the CanvasScene load() needs is far more machinery for the same
    // assertion.
    static QStringList sanitize_item_graph(QJsonObject& root);

    // Repairs the transform numbers of every item in place - a scale
    // that is non-positive or absurd, a coordinate far outside any real
    // scene, an unnormalized angle - and returns one message per repair
    // that is actually corruption (an unnormalized angle is not, so it
    // is fixed silently; see the definition). Nothing downstream checks
    // these: create_item_from_data() hands them to QGraphicsItem's own
    // setters directly, which have no guards of their own.
    //
    // Called by load(); public for the same reason as the above.
    static QStringList sanitize_item_transforms(QJsonObject& root);
};

#endif // FML_ARCHIVE_H

#ifndef EXPORT_H
#define EXPORT_H

// For PixmapItem::ExportSource, which the snapshots below hold by
// value - it is the whole point of this header's threading rule.
#include "moveitem.h"

#include <QImage>
#include <QList>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <memory>

class CanvasScene;
class TextItem;
class QWidget;
class ThreadedIO;

// Wraps ThreadedIO signal emission with null-worker safety, since an
// exporter can run synchronously too (worker == nullptr) as well as
// backgrounded via ThreadedIO.
class ExporterBase
{
public:
    virtual ~ExporterBase() = default;

    // Same reasoning as ValueHandler (core/valuehandler.h).
protected:
    // Declaring the copy/move operations below suppresses the
    // implicit default constructor, which derived classes rely on.
    ExporterBase() = default;
    ExporterBase(const ExporterBase&) = default;
    ExporterBase& operator=(const ExporterBase&) = default;
    ExporterBase(ExporterBase&&) = default;
    ExporterBase& operator=(ExporterBase&&) = default;

public:
protected:
    static void emit_begin_processing(ThreadedIO* worker, int total);
    static void emit_progress(ThreadedIO* worker, int value);
    static void emit_finished(ThreadedIO* worker,
                              const QString& target,
                              const QStringList& errors);
    static void emit_user_input_required(ThreadedIO* worker,
                                         const QString& message);
};

// For exporting the whole scene to a single image file.
//
// THREADING - the rule every exporter in this file obeys:
//
//   prepare()   runs on the GUI thread and is the ONLY place allowed to
//               touch the scene, its items or their QPixmaps. It takes
//               a snapshot of everything the export needs.
//   export_to() runs on a ThreadedIO worker and reads nothing but that
//               snapshot.
//
// It used to be one step, run entirely on the worker, which meant
// QGraphicsScene::render() walked live items while the GUI thread
// painted the same ones. Three concrete races came out of that:
// PixmapItem::paint() writing its mutable downscaleCache_ from both
// threads, pixmap_to_bytes() writing encodedCache_ from the worker
// while the autosave/recovery timers wrote it from the GUI thread
// (QTimer keeps firing under a WindowModal dialog), and a GIF frame
// tick replacing an item's pixmap mid-render. Qt does not warn about
// any of it: it only prints "not safe to use pixmaps outside the GUI
// thread" on platforms without QPlatformIntegration::ThreadedPixmaps,
// which xcb has.
//
// Cancels active modes and deselects everything on construction so
// selection outlines/handles never end up in the export, and puts the
// selection back on destruction.
class SceneExporterBase : public ExporterBase
{
public:
    explicit SceneExporterBase(CanvasScene* scene);
    ~SceneExporterBase() override;

    SceneExporterBase(const SceneExporterBase&) = delete;
    SceneExporterBase& operator=(const SceneExporterBase&) = delete;
    SceneExporterBase(SceneExporterBase&&) = delete;
    SceneExporterBase& operator=(SceneExporterBase&&) = delete;

    // Asks the user for whatever this export type still needs (e.g. a
    // target pixel size); returns false if the user canceled.
    virtual bool get_user_input(QWidget* parent) = 0;
    // GUI thread, after get_user_input() and before the worker starts.
    virtual void prepare() = 0;
    virtual void export_to(const QString& filename, ThreadedIO* worker) = 0;

protected:
    CanvasScene* scene() const { return scene_; }
    // Scene's itemsBoundingRect() rounded to a QSize, grown by a 3%
    // margin - the size offered by SceneToPixmapExporterDialog and used
    // unconditionally by SceneToSVGExporter (which doesn't ask). Both are
    // computed once in this class's constructor; subclasses only read them.
    QSize default_size() const { return defaultSize_; }
    qreal margin() const { return margin_; }

private:
    CanvasScene* scene_;
    QSize defaultSize_;
    qreal margin_ = 0;
    // By uid, not by pointer: the restore happens after the export has
    // finished, and resolving through CanvasScene::find_by_uid() means
    // an item that disappeared in between is simply not reselected
    // rather than dereferenced.
    QList<QUuid> selectedUids_;
};

class SceneToPixmapExporter : public SceneExporterBase
{
public:
    using SceneExporterBase::SceneExporterBase;

    bool get_user_input(QWidget* parent) override;
    // The target pixel size, normally answered by the dialog
    // get_user_input() puts up. Exposed so an export that has no user
    // to ask - a test, or a future batch/CLI path - can still say what
    // it wants before prepare() renders.
    void set_size(QSize size) { size_ = size; }
    // Renders the scene here, on the GUI thread. The worker is left
    // with the encode-and-write half, which is the slow one for a large
    // PNG or JPEG anyway.
    void prepare() override;
    void export_to(const QString& filename, ThreadedIO* worker) override;

private:
    QImage render_to_image() const;

    QSize size_;
    QImage rendered_;
};

class SceneToSVGExporter : public SceneExporterBase
{
public:
    using SceneExporterBase::SceneExporterBase;

    // Everything the SVG writer used to read straight off a live
    // QGraphicsItem, flattened to values. The picture bytes are the
    // expensive part and stay unencoded here on purpose: `source`
    // carries whatever is needed to produce them later.
    struct Node
    {
        bool isText = false;
        QPointF pos;
        QPointF anchor;
        qreal rotation = 0;
        qreal flip = 1;
        qreal opacity = 1;
        // Text only.
        QString style;
        QString text;
        // Picture only.
        qreal width = 0;
        qreal height = 0;
        bool crispEdges = false;
        PixmapItem::ExportSource source;
    };

    // No dialog - always exports at defaultSize_.
    bool get_user_input(QWidget* parent) override;
    // Walks the scene here; the worker only encodes the pictures and
    // writes the XML out.
    void prepare() override;
    void export_to(const QString& filename, ThreadedIO* worker) override;

    // The worker half, written as what it is: a pure function of the
    // snapshot, with no exporter, scene or item reachable from it.
    static QString nodes_to_svg(const QList<Node>& nodes,
                                QSize size,
                                ThreadedIO* worker = nullptr);

    const QList<Node>& nodes() const { return nodes_; }

private:
    static QString text_styles(TextItem* item);

    QSize size_;
    QList<Node> nodes_;
};

// Picks an exporter by file extension
// exporter_registry[ext] dict lookup: "svg" (case-insensitive) gets
// SceneToSVGExporter, anything else (including no/unknown extension)
std::unique_ptr<SceneExporterBase> create_scene_exporter(
    const QString& extension, CanvasScene* scene);

// Exports every pixmap item in the scene to its own file in a
// directory. Resumable: exportTo() returns early via
// ExporterBase::emitUserInputRequired() the first time a target file
// already exists and handleExisting() hasn't been set yet: the caller
// shows a conflict-resolution dialog, calls setHandleExisting(), and
// calls exportTo() again to resume from exactly that item.
class ImagesToDirectoryExporter : public ExporterBase
{
public:
    ImagesToDirectoryExporter(CanvasScene* scene, const QString& dirname);
    // Explicit subset (HierarchyPanel's per-picture/per-group "Export"
    // context menu item) instead of every pixmap item in the scene.
    ImagesToDirectoryExporter(const QList<PixmapItem*>& items,
                              const QString& dirname);

    const QString& dirname() const { return dirname_; }
    // One of "skip"/"skip_all"/"overwrite"/"overwrite_all" (see
    // ExportImagesFileExistsDialog::getAnswer()).
    void set_handle_existing(const QString& policy)
    {
        handleExisting_ = policy;
    }

    // One snapshotted picture: where it goes and what to write there.
    struct Job
    {
        QString filename;
        // Non-empty for a GifItem, whose own bytes are stored verbatim
        // so the animation is not flattened to one still frame.
        QByteArray gifBytes;
        PixmapItem::ExportSource source;
    };

    // GUI thread, once, before the worker starts - see the threading
    // note on SceneExporterBase above. The conflict pause does NOT come
    // back through here: the jobs are already captured and startFrom_
    // says where to pick up, so a resume goes straight to export_to().
    void prepare();
    void export_to(ThreadedIO* worker = nullptr);

    // Testing seam: lets a test confirm the worker half really is
    // independent of the items by dropping them between the two steps.
    const QList<Job>& jobs() const { return jobs_; }

private:
    QList<PixmapItem*> items_;
    QList<Job> jobs_;
    QString dirname_;
    int startFrom_ = 0;
    QString handleExisting_;
};

#endif // EXPORT_H

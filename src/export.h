#ifndef EXPORT_H
#define EXPORT_H

#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

#include <memory>

class CanvasScene;
class PixmapItem;
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

protected:
    void emitBeginProcessing(ThreadedIO* worker, int total) const;
    void emitProgress(ThreadedIO* worker, int value) const;
    void emitFinished(ThreadedIO* worker,
                      const QString& target,
                      const QStringList& errors) const;
    void emitUserInputRequired(ThreadedIO* worker, const QString& message) const;
};

// For exporting the whole scene to a single image file. Cancels active
// modes and deselects everything on construction so
// selection outlines/handles never end up in the export.
class SceneExporterBase : public ExporterBase
{
public:
    explicit SceneExporterBase(CanvasScene* scene);

    // Asks the user for whatever this export type still needs (e.g. a
    // target pixel size); returns false if the user canceled.
    virtual bool getUserInput(QWidget* parent) = 0;
    virtual void exportTo(const QString& filename, ThreadedIO* worker = nullptr)
        = 0;

protected:
    CanvasScene* scene_;
    // Scene's itemsBoundingRect() rounded to a QSize, grown by a 3%
    // margin - the size offered by SceneToPixmapExporterDialog and used
    // unconditionally by SceneToSVGExporter (which doesn't ask).
    QSize defaultSize_;
    qreal margin_ = 0;
};

class SceneToPixmapExporter : public SceneExporterBase
{
public:
    using SceneExporterBase::SceneExporterBase;

    bool getUserInput(QWidget* parent) override;
    void exportTo(const QString& filename,
                  ThreadedIO* worker = nullptr) override;

private:
    QImage renderToImage() const;

    QSize size_;
};

class SceneToSVGExporter : public SceneExporterBase
{
public:
    using SceneExporterBase::SceneExporterBase;

    // No dialog - always exports at defaultSize_.
    bool getUserInput(QWidget* parent) override;
    void exportTo(const QString& filename,
                  ThreadedIO* worker = nullptr) override;

private:
    QString renderToSvg(ThreadedIO* worker) const;
    QString textStyles(TextItem* item) const;

    QSize size_;
};

// Picks an exporter by file extension
// exporter_registry[ext] dict lookup: "svg" (case-insensitive) gets
// SceneToSVGExporter, anything else (including no/unknown extension)
std::unique_ptr<SceneExporterBase> createSceneExporter(const QString& extension,
                                                       CanvasScene* scene);

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
    void setHandleExisting(const QString& policy) { handleExisting_ = policy; }

    void exportTo(ThreadedIO* worker = nullptr);

private:
    QList<PixmapItem*> items_;
    QString dirname_;
    int startFrom_ = 0;
    QString handleExisting_;
};

#endif // EXPORT_H

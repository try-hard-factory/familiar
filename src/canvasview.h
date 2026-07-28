#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <functional>
#include <memory>
#include <optional>
#include <widgets/main_controls.h>
#include <widgets/welcome_overlay.h>
#include <QGraphicsView>
#include <QMimeData>
#include <QMouseEvent>
#include <QObject>
#include <QScrollBar>
#include <QTransform>
#include <QUrl>
#include <QWheelEvent>

class MainWindow;
class project_settings;
class CanvasScene;
class QUndoStack;
class IBaseItem;
class PixmapItem;
class SampleColorWidget;
class ThreadedIO;
class SceneExporterBase;
class ImagesToDirectoryExporter;

struct PreviousTransform
{
    QGraphicsItem* toggleItem;
    QTransform transform;
    QPointF center;
};

class CanvasView : public MainControlsMixin<CanvasView, QGraphicsView>
{
    Q_OBJECT
public:
    enum ActiveMode { ModeNone, ModePan, ModeZoom, ModeSampleColor };

    CanvasView(MainWindow& mw, QWidget* parent = nullptr);
    ~CanvasView();

    void setProjectSettings(project_settings* ps);
    void do_insert_images(const QList<QUrl>& urls,
                          std::optional<QPoint> pos = std::nullopt);
    void handleDrop(const QMimeData* mimedata, const QPoint& pos);

    // Used by MainWindow to (re)wire action enabled-state to whichever
    // tab is currently active (see MainWindow::resyncActionsForTab).
    CanvasScene* scene() const { return scene_; }
    QUndoStack* undoStack() const { return undoStack_.get(); }

    qreal get_scale() const { return transform().m11(); }
    QPointF getViewCenter() const;
    void resetPreviousTransform(QGraphicsItem* toggleItem = nullptr);
    void fitRect(const QRectF& rect, QGraphicsItem* toggleItem = nullptr);

    void cancelActiveModes();
    void cancelSampleColorMode();

    void cleanupWorkplace();
    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();

    // The drawn canvas frame's extent - see canvasRect_ and
    // CanvasScene::rememberedBoundingRect().
    QRectF canvasRect() const { return canvasRect_; }
    // Applied right after loading a project (see FileActions::
    // loadFmlIntoCurrentTab()): seeds canvasRect_ from the manifest's
    // stored value and marks this tab as an existing (not fresh/
    // untitled) scene, regardless of whether it currently has any items.
    void restoreCanvasRect(const QRectF& rect)
    {
        canvasRect_ = rect;
        sceneEverHadItems_ = true;
        // Reuses on_scene_changed() rather than duplicating its overlay-
        // show/hide and focus logic here: with sceneEverHadItems_ already
        // true and (typically) zero items at this point, it'll leave
        // canvasRect_ as just set and make sure the welcome overlay is
        // hidden - on_scene_changed() otherwise never runs for a load
        // that added no items (add_queued_items() calling addItem()
        // zero times never fires QGraphicsScene::changed()).
        on_scene_changed();
    }

public slots:
    void on_scene_changed();
    void on_selection_changed();
    void on_context_menu(const QPoint& point);
    void on_cursor_changed(QCursor cursor);
    void on_cursor_cleared();
    void on_undo_clean_changed(bool clean);
    void settingsChangedSlot();

public:
    // Per-tab action bodies. No longer QMetaObject::invokeMethod-driven
    // slots: MainWindow now owns the single QAction set (see
    // ActionsMixin<QMainWindow> there) and forwards to
    // tabpane_->currentWidget() for whichever of these applies to the
    // active tab. Plain methods, called by ordinary C++ call from there.

    // File
    void on_action_save();
    void on_action_save_as();
    void on_action_export_scene();
    void on_action_export_images();

    // Edit
    void on_action_undo();
    void on_action_redo();
    void on_action_select_all();
    void on_action_deselect_all();
    void on_action_cut();
    void on_action_copy();
    void on_action_paste();
    void on_action_delete_items();
    void on_action_raise_to_top();
    void on_action_lower_to_bottom();

    // View
    void on_action_fit_scene();
    void on_action_fit_selection();

    // Insert
    void on_action_insert_images();
    void on_action_insert_text();

    // Transform
    void on_action_crop();
    void on_action_flip_horizontally();
    void on_action_flip_vertically();
    void on_action_reset_scale();
    void on_action_reset_rotation();
    void on_action_reset_flip();
    void on_action_reset_crop();
    void on_action_reset_transforms();

    // Normalize
    void on_action_normalize_height();
    void on_action_normalize_width();
    void on_action_normalize_size();

    // Arrange
    void on_action_arrange_optimal();
    void on_action_arrange_horizontal();
    void on_action_arrange_vertical();
    void on_action_arrange_square();

    // Images
    void on_action_change_opacity();
    void on_action_grayscale();
    void on_action_show_color_gamut();
    void on_action_sample_color();

private slots:
    // do_insert_images callbacks (see fileio::load_images / ThreadedIO).
    // Mirrors Python's CanvasView.on_items_loaded/on_insert_images_finished.
    void on_items_loaded(int value);
    void on_insert_images_finished(const QString& filename,
                                   const QStringList& errors);

    void on_export_scene_finished(const QString& filename,
                                  const QStringList& errors);
    void on_export_images_file_exists(const QString& filename);
    void on_export_images_finished(const QString& dirname,
                                   const QStringList& errors);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void dropEvent(QDropEvent* event) override;

private:
    void recalcSceneRect();
    void doScale(qreal sx, qreal sy);
    double getZoomSize(std::function<double(double, double)> func) const;
    void zoom(double delta, QPointF anchor);
    void pan(QPointF delta);
    QString getSupportedImageFormats() const;

    MainWindow& mainwindow_;
    WelcomeOverlay* welcomeOverlay_;
    std::unique_ptr<QUndoStack> undoStack_;
    CanvasScene* scene_;
    uint64_t zCounter_ = 0;

    ActiveMode activeMode_ = ModeNone;
    QPointF eventStart_;
    QPointF eventAnchor_;
    bool eventInverted_ = false;
    // Live color-preview swatch shown while ModeSampleColor is active;
    // owned here (not self-deleting) since it must survive across
    // multiple mouseMoveEvent updates, unlike FamNotification.
    SampleColorWidget* sampleColorWidget_ = nullptr;

    std::unique_ptr<PreviousTransform> previousTransform_;

    // Set by on_action_cut() right before removing the cut items, so the
    // resulting (transient) empty-scene state doesn't reset the view's
    // zoom in on_scene_changed(). Only matters pre-sceneEverHadItems_
    // (see below) - once a scene has had content, going back to zero
    // items never resets the view at all regardless of this flag.
    // scene_->changed() is emitted asynchronously (Qt batches updates),
    // so this can't be a simple before/after-push() bracket: it's
    // consumed - cleared to false - the very next time on_scene_changed()
    // runs, whatever the outcome, so it suppresses exactly one reaction
    // and nothing beyond that even if paste (or nothing at all) follows
    // much later.
    bool suppressNextEmptySceneReset_ = false;

    // Latches true the first time this tab's scene has >=1 item, never
    // reset back to false (no in-place "clear this tab back to blank"
    // action exists - see CanvasScene::clear(), only called from its own
    // constructor). Gates the empty-scene handling in on_scene_changed():
    // a tab that's had content is still "the same project", just emptied
    // out by deleting everything - unlike a genuinely fresh/untitled tab,
    // it should keep its view/canvas frame as-is and not resurface the
    // welcome/recent-files overlay.
    bool sceneEverHadItems_ = false;

    // The visible "canvas" rect drawn in drawBackground(): grows to
    // include items as they're added/moved (united() only ever expands),
    // but never shrinks back on its own - matching how it used to look
    // when drawBackground read QGraphicsScene::sceneRect() (which has
    // that exact "grows but never shrinks" behavior built in). Reset to
    // empty the moment the scene has zero items, but (see
    // sceneEverHadItems_ above) only for a tab that never had content in
    // the first place - deliberately does stay stuck at the largest-ever
    // item bounds otherwise now, which is what a previous fix here
    // avoided in general (see git history) but Max wants specifically
    // for "this project's tab, now emptied out" (2026-07-26). Updated in
    // on_scene_changed().
    QRectF canvasRect_;

    // Right-click: drag → move window, click → context menu
    bool rightPressed_ = false;
    bool rightDragging_ = false;
    QPoint rightPressPos_;
    QPoint rightWndPos_;

    QColor canvasColor_;
    QColor borderColor_;
    int currentOpacity_;

    // State for the in-flight do_insert_images() operation, read by
    // on_items_loaded()/on_insert_images_finished(). Assumes at most one
    // insert-images operation runs at a time (matches Python storing
    // these on self.worker/instance state directly).
    bool insertImagesNewScene_ = false;
    QList<IBaseItem*> insertImagesInsertedItems_;

    // State for the in-flight on_action_export_scene() operation - kept
    // alive across the async ThreadedIO call, released in
    // on_export_scene_finished(). See src/export.h.
    std::unique_ptr<SceneExporterBase> sceneExporter_;

    // State for the in-flight on_action_export_images() operation.
    // ImagesToDirectoryExporter (src/export.h) is resumable: exportTo()
    // pauses via ThreadedIO::userInputRequired when a target file
    // already exists, and resumes (same worker, QThread::start() again)
    // once the user picks a conflict policy in
    // on_export_images_file_exists().
    std::unique_ptr<ImagesToDirectoryExporter> imagesExporter_;
    ThreadedIO* imageExportWorker_ = nullptr;
};

#endif // CANVASVIEW_H

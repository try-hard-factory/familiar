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
    void addImage(const QString& path, QPointF point);
    void addImage(QImage* img, QPointF point);
    void addImage(QByteArray ba, int w, int h, QRect br, qsizetype bpl, QImage::Format f);

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

    QByteArray fml_payload();
    void cleanupWorkplace();
    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();

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
    void on_action_show_scrollbars(bool checked);

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
    void on_action_grayscale(bool checked);
    void on_action_show_color_gamut();
    void on_action_sample_color();

private slots:
    // do_insert_images callbacks (see fileio::load_images / ThreadedIO).
    // Mirrors Python's CanvasView.on_items_loaded/on_insert_images_finished.
    void on_items_loaded(int value);
    void on_insert_images_finished(const QString& filename,
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

    MainWindow& mainwindow_;
    WelcomeOverlay* welcomeOverlay_;
    std::unique_ptr<QUndoStack> undoStack_;
    CanvasScene* scene_;
    uint64_t zCounter_ = 0;

    ActiveMode activeMode_ = ModeNone;
    QPointF eventStart_;
    QPointF eventAnchor_;
    bool eventInverted_ = false;

    std::unique_ptr<PreviousTransform> previousTransform_;

    // Right-click: drag → move window, click → context menu
    bool rightPressed_  = false;
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
    QStringList insertImagesImmediateErrors_;
    QList<IBaseItem*> insertImagesInsertedItems_;
};

#endif // CANVASVIEW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/**
 *  @file   mainwindow.h
 *  \~russian @brief  Класс главного окна QT
 *  \~russian @author max aka angeleyes (mpano91@gmail.com)
 *  \~russian @bug пока нет:).
 *
 *  \~english @brief  Main window QT class
 *  \~english @author max aka angeleyes (mpano91@gmail.com)
 *  \~english @bug No known bugs.
 */

#include "file_actions.h"
#include "tabpane.h"
#include <actions/action_mixin.h>
#include <canvasview.h>
#include <core/settingshandler.h>
#include <utils/utils.h>
#include <QApplication>
#include <QCursor>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QWindow>
class project_settings;
class QFileDialog;
class SaveAllWindow;
class QShortcut;

enum EShortcutButtons {
    k_TYPE_NEW = 0,
    k_TYPE_OPEN = 1,
    k_TYPE_SAVE = 2,
    k_TYPE_QUIT = 3,
    k_ALL = 4
};

constexpr QPoint kInvalidPoint(-1, -1);

class MainWindow : public ActionsMixin<QMainWindow>
{
    Q_OBJECT
public:
    /**
     * \~russian @brief конструктор
     * \~russian @param parent - указатель на QWidget(может быть nullptr - это
     *                           нормально)
     *
     * \~english @brief main window class constructor
     * \~english @param parent - pointer to QWidget parent(may be nullptr - it
     *                           is normal)
     */
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void quitProject();
    void saveAllWindowSaveCB(SaveAllWindow* w, std::map<int, bool>&& m);
    void cleanupWorkplace();
    void exitProject();
    TabPane& tabPane();
    FileActions& fileActions();

    void clipboardItems(QVector<QGraphicsItem*> ci) noexcept
    {
        clipboardItems_ = ci;
    }
    QVector<QGraphicsItem*>& clipboardItems() noexcept
    {
        return clipboardItems_;
    }
    void clearClipboardItems() { clipboardItems_.clear(); }

public slots:
    // Window-level actions: MainWindow owns the single, app-wide QAction
    // set (via ActionsMixin) since the menu bar/shortcuts are shared
    // across all tabs. These have real bodies here.

    // File
    void on_action_new_scene();
    void on_action_open();
    void on_action_open_recent_file(const QString& filename);
    void on_action_quit();

    // View
    void on_action_fullscreen(bool checked);
    void on_action_always_on_top(bool checked);
    void on_action_show_menubar(bool checked);
    void on_action_show_titlebar(bool checked);
    void on_action_move_window();

    // Settings / Help
    void on_action_settings();
    void on_action_keyboard_settings();
    void on_action_open_settings_dir();
    void on_action_help();
    void on_action_about();
    void on_action_debuglog();

    // Per-tab actions: thin forwarders to tabpane_->currentWidget().
    // Real logic stays on CanvasView; see there for implementations.

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

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

protected:
    void mouseMoveEvent(QMouseEvent* event) override
    {
        updateResizeCursor(event->pos());
        QMainWindow::mouseMoveEvent(event);
    }

    // Central widget (and children like menuBar()) cover the whole frameless
    // window, so QMainWindow rarely gets its own mouse events while hovering
    // or clicking over them. Watch every mouse move/press application-wide so
    // the resize cursor and the drag-to-resize border work regardless of
    // which child widget actually received the event.
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            updateResizeCursor(
                mapFromGlobal(mouseEvent->globalPosition().toPoint()));
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                tryStartSystemResize(
                    mapFromGlobal(mouseEvent->globalPosition().toPoint()));
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

private:
    static constexpr int kResizeBorder
        = 10; // Толщина невидимой границы для ресайза, в пикселях

    Qt::Edges resizeEdgesAt(const QPoint& pos) const
    {
        Qt::Edges edges;
        if (pos.x() < kResizeBorder)
            edges |= Qt::LeftEdge;
        if (pos.x() > width() - kResizeBorder)
            edges |= Qt::RightEdge;
        if (pos.y() < kResizeBorder)
            edges |= Qt::TopEdge;
        if (pos.y() > height() - kResizeBorder)
            edges |= Qt::BottomEdge;
        return edges;
    }

    void tryStartSystemResize(const QPoint& pos)
    {
        if (!rect().contains(pos) || !windowHandle())
            return;

        // Запуск нативного изменения размера (доступно в Qt 5.15 и новее)
        const Qt::Edges edges = resizeEdgesAt(pos);
        if (edges) {
            windowHandle()->startSystemResize(edges);
        }
    }

    void updateResizeCursor(const QPoint& pos)
    {
        if (!rect().contains(pos)) {
            unsetCursor();
            return;
        }

        const Qt::Edges edges = resizeEdgesAt(pos);

        if ((edges & Qt::LeftEdge && edges & Qt::TopEdge)
            || (edges & Qt::RightEdge && edges & Qt::BottomEdge))
            setCursor(Qt::SizeFDiagCursor);
        else if ((edges & Qt::RightEdge && edges & Qt::TopEdge)
                 || (edges & Qt::LeftEdge && edges & Qt::BottomEdge))
            setCursor(Qt::SizeBDiagCursor);
        else if (edges & (Qt::LeftEdge | Qt::RightEdge))
            setCursor(Qt::SizeHorCursor);
        else if (edges & (Qt::TopEdge | Qt::BottomEdge))
            setCursor(Qt::SizeVerCursor);
        else
            unsetCursor();
    }

private:
    QPoint pos_ = kInvalidPoint;

private:
    bool checkSave();

public slots:
    void settingsChangedSlot();
private slots:
    void saveAll();
    void newFile();
    void settingsWindow();
    void saveFile();
    void quit();
    void openFile();
    void saveFileAs();
    void notifyShortcut(const QString& t);

private slots:
    // Resync the shared action enabled-state to whichever tab is now
    // active (connected to TabPane::currentTabChanged). Everything else
    // in this group reacts to the currently-hooked tab's scene_/
    // undoStack_ signals - see resyncActionsForTab().
    void onCurrentTabChanged(int index);
    void on_active_scene_changed();
    void on_active_selection_changed();
    void on_active_can_undo_changed(bool canUndo);
    void on_active_can_redo_changed(bool canRedo);

private:
    // (Re)connects scene_/undoStack_ signals from `cv` to the slots
    // above, disconnecting the previously-hooked tab first, then
    // immediately pushes `cv`'s current state into actiongroup_set_enabled
    // - these are edge-triggered signals with no "replay current value"
    // on connect. `cv` may be null for the brief moment TabPane::
    // onTabClosed() has zero tabs while replacing the last closed one.
    void resyncActionsForTab(CanvasView* cv);

    // QPointer, not raw CanvasScene*/QUndoStack* (or the owning
    // CanvasView*): this can be re-entered from inside the old tab's own
    // ~CanvasView() (Qt emits currentChanged(-1) synchronously while
    // QTabWidget is removing the about-to-be-destroyed widget). By then
    // ~CanvasView()'s body has already run `delete scene_`, so a raw
    // pointer captured earlier would dangle; QPointer nulls itself out
    // the moment the tracked QObject's destructor runs.
    QPointer<CanvasScene> hookedScene_;
    QPointer<QUndoStack> hookedUndoStack_;

    SettingsHandler settings_;
    FileActions* fileactions_ = nullptr;
    TabPane* tabpane_ = nullptr;
    QVector<QGraphicsItem*> clipboardItems_;

    int currentOpacity_;
    QColor backGroundColor_;
    QString rgbaBackGroundStr_;
};

#endif // MAINWINDOW_H

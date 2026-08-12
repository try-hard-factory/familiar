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
#include <utils/utils.h>
#include <QApplication>
#include <QCursor>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QWindow>
class project_settings;
class QFileDialog;
class SaveAllDialog;
class QShortcut;
class QMenuBar;
class QGraphicsOpacityEffect;
class QVariantAnimation;
class QTimer;
class HierarchyPanel;

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
    void saveAllWindowSaveCB(SaveAllDialog* w, std::map<int, bool>&& m);
    void cleanupWorkplace();
    void exitProject();
    TabPane& tabPane();
    FileActions& fileActions();

    // Call after any scene mutation that bypasses the undo stack (e.g. a
    // background file load's add_queued_items() - see hierarchy_panel.h's
    // own comment) so the Hierarchy panel picks it up. Normal
    // command-driven mutations don't need this - they're already covered
    // by hookedUndoStack_'s indexChanged, hooked in resyncActionsForTab().
    void notifyStructuralChange();

    // Call once from main() INSTEAD OF a bare show() - if
    // familiar::recovery::scan() finds anything left over from a session
    // that didn't exit cleanly, this shows only RecoveryDialog first
    // (parented to nullptr, since this window isn't visible yet) and
    // defers actually show()'ing this window until that dialog is
    // dismissed, however the user dismissed it - otherwise the main
    // window (with its pointless default blank tab) would flash up
    // behind the recovery prompt before the user has even decided
    // anything. Shows immediately, same as a plain show(), if there's
    // nothing to recover.
    //
    // startupFile: the -f/positional CLI path (main.cpp), if any -
    // opened via FileActions::processOpenFile() only AFTER the recovery
    // dialog (if any) has actually been dismissed, not immediately.
    // processOpenFile() itself already no-ops/switches-tab if that path
    // is already open under an existing tab - so if the user restores a
    // recovery snapshot for that same file from the dialog, this
    // naturally lands on the restored tab instead of ALSO loading a
    // stale duplicate straight from disk (the recovery dialog should
    // still show up as normal, just without ending up with two tabs
    // for the same file afterward).
    void showOrOfferRecovery(const QString& startupFile = QString());

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
    void on_action_auto_hide_ui(bool checked);
    void on_action_hierarchy(bool checked);

    // Settings / Help
    void on_action_settings();
    void on_action_settings_new();
    void on_action_keyboard_settings();
    void on_action_open_settings_dir();
    void on_action_help();
    void on_action_about();
    void on_action_debuglog();

    // HelpDialog's "Keyboard Shortcuts" link - opens the (new) settings
    // window already on that category, not just settingsWindowNew()'s
    // own default/last-used one.
    void openKeyboardShortcutsSettings();

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
    void on_action_group();
    void on_action_ungroup();

    // View
    void on_action_fit_scene();
    void on_action_fit_selection();
    void on_action_zoom_in();
    void on_action_zoom_out();

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

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

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
        if (event->type() == QEvent::MouseMove
            || event->type() == QEvent::MouseButtonPress) {
            // This is an application-wide filter (qApp->installEventFilter),
            // so `watched` can belong to an entirely different top-level
            // window overlapping this one on screen - e.g. a QColorDialog
            // opened from the floating text toolbar, which tends to land
            // near the top of the screen, right over the resize border/
            // menu bar drag zone. mapFromGlobal() below has no idea the
            // click wasn't meant for us; without this guard, such a click
            // gets silently reinterpreted as a resize/window-drag and the
            // dialog underneath never sees it.
            auto* w = qobject_cast<QWidget*>(watched);
            if (!w || w->window() != this)
                return QMainWindow::eventFilter(watched, event);
        }

        if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint pos = mapFromGlobal(
                mouseEvent->globalPosition().toPoint());
            updateResizeCursor(pos);
            handleUiHover_(pos);
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint pos = mapFromGlobal(
                mouseEvent->globalPosition().toPoint());
            // Resize wins over drag: the top kResizeBorder pixels overlap
            // the menu bar, and the thin border is harder to hit.
            if (mouseEvent->button() == Qt::LeftButton
                && (tryStartSystemResize(pos) || tryStartWindowDrag_(pos))) {
                return true;
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

    // Returns true if a resize was actually started, so the caller can
    // consume the press event - otherwise it falls through to whatever's
    // underneath (e.g. CanvasScene), which would start a rubber-band
    // selection for the same click.
    bool tryStartSystemResize(const QPoint& pos)
    {
        if (!rect().contains(pos) || !windowHandle())
            return false;

        // Запуск нативного изменения размера (доступно в Qt 5.15 и новее)
        const Qt::Edges edges = resizeEdgesAt(pos);
        if (edges) {
            windowHandle()->startSystemResize(edges);
            return true;
        }
        return false;
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
    // Periodic autosave. onAutosaveTimeout_() fires
    // every autosaveTimer_ tick; restartAutosaveTimer_() re-reads
    // Save/autosave_enabled + Save/autosave_interval_seconds fresh and
    // (re)starts or stops the timer accordingly - called once at startup
    // and again live whenever either setting changes (see
    // SettingsEvents::autosaveSettingsChanged).
    void onAutosaveTimeout_();
    void restartAutosaveTimer_();

    // Crash recovery - independent of the above and
    // NOT gated by Save/autosave_enabled: it writes into its own
    // recovery/ folder, not the tab's real file, so it's always-on
    // regardless of whether the user wants their real files overwritten
    // periodically. Snapshots every modified tab (titled or not - see
    // recovery.h). The startup check itself is showOrOfferRecovery()
    // above, called explicitly from main() rather than from in here -
    // see its own comment for why.
    void onRecoveryTimeout_();
    void saveAll();
    void newFile();
    void settingsWindow();
    void settingsWindowNew();
    void saveFile();
    void quit();
    void openFile();
    void saveFileAs();

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

    // Scene outliner dock (ui/hierarchy_panel.h) - one long-lived
    // instance, rebound to whichever tab is active from within
    // resyncActionsForTab() rather than one per CanvasView.
    HierarchyPanel* hierarchyPanel_ = nullptr;

    // ── Menu bar overlay (see mainwindow.cpp, "Menu bar" section) ─────────
    // The menu bar is a long-lived direct child laid at the top of the
    // window, NOT installed via QMainWindow::setMenuBar() -
    // setMenuBar(nullptr) deleteLater()s the old bar, which would forbid
    // hiding/showing the same instance. Auto-hide ("auto_hide_ui") fades
    // the menu bar AND the tab bar out when the cursor leaves the top
    // strip; both keep their layout space, so nothing underneath ever
    // moves or gets covered.
    void ensureMenubar_();
    void applyMenubarState_();
    void updateMenubarGeometry();
    void updateWindowControlsStyle_();
    void startUiFade_(bool visible);
    void onUiHideTimeout_();
    void handleUiHover_(const QPoint& pos);
    bool tryStartWindowDrag_(const QPoint& pos);
    bool uiStripContains_(const QPoint& pos) const;
    int uiStripHeight_() const;

    QMenuBar* menubar_ = nullptr;
    // Corner widget with the window-control buttons; restyled from the
    // current color preset on every settings change.
    QWidget* windowControls_ = nullptr;
    QGraphicsOpacityEffect* menubarOpacity_ = nullptr;
    QGraphicsOpacityEffect* tabbarOpacity_ = nullptr;
    QVariantAnimation* uiFadeAnim_ = nullptr;
    QTimer* uiHideTimer_ = nullptr;
    bool autoHideUi_ = false;
    // Logical shown/hidden state of the auto-hidden UI: the target of
    // the running (or last finished) fade.
    bool uiFadeTargetVisible_ = true;
    // Current fade opacity, mirrored out of the animation so paintEvent
    // can dim the window's own background fill under the top strip in
    // sync with the fading widgets.
    qreal uiOpacity_ = 1.0;

    FileActions* fileactions_ = nullptr;
    TabPane* tabpane_ = nullptr;
    QTimer* autosaveTimer_ = nullptr;
    QTimer* recoveryTimer_ = nullptr;
    QVector<QGraphicsItem*> clipboardItems_;

    int currentOpacity_;
    QColor backGroundColor_;
    QString rgbaBackGroundStr_;
};

#endif // MAINWINDOW_H

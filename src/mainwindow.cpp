#include "mainwindow.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>

#include "canvasscene.h"
#include "project_settings.h"
#include "saveallwindow.h"
#include "tabpane.h"
#include <actions/action_mouse_dispatch.h>
#include <core/held_buttons_tracker.h>
#include <core/settingshandler.h>
#include <map>
#include <ui/new_settings_window.h>
#include <ui/settings_window.h>
#include <QUndoStack>

#include "log/log.h"
using namespace familiar::log;

MainWindow::MainWindow(QWidget* parent)
    // ActionsMixin<T>'s constructor only forwards `parent` (no extra
    // QMainWindow flags argument); the custom flags this used to pass
    // are unconditionally overwritten later in this same constructor by
    // setWindowFlags(Qt::Window | Qt::FramelessWindowHint) below, so
    // dropping them here is not a behavior change.
    : ActionsMixin<QMainWindow>(parent)
    , fileactions_(new FileActions(*this))
{
    // tabpane_ is built here (constructor body), not via the
    // member-initializer list: building it constructs the first
    // CanvasView. Constructing it too early, before this class's own
    // setup is done, previously let Qt Designer's generated setupUi()
    // silently discard the real menu bar right after it was set (now
    // moot since setupUi() is gone, but the ordering still matters for
    // build_menu_and_actions() below).
    tabpane_ = new TabPane(this, *this);
    build_menu_and_actions();
    connect(tabpane_,
            &TabPane::currentTabChanged,
            this,
            &MainWindow::onCurrentTabChanged);
    // The first tab's own currentChanged(0) already fired inside
    // TabPane's constructor, before the connect() above existed - sync
    // manually here so the initial action enabled-state is correct.
    resyncActionsForTab(tabpane_->currentWidget());
    setWindowTitle("Familiar");
    // mainwindow.ui used to set this via its geometry property (800x600);
    // now that ui->setupUi() is gone, set a sane default size explicitly.
    resize(800, 600);
    statusBar()->hide();

    setMouseTracking(true);
    //this->setWindowFlags(Qt::WindowTransparentForInput|Qt::WindowStaysOnTopHint);

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &MainWindow::settingsChangedSlot);
    settingsChangedSlot();

    tabpane_->setWindowFlags(Qt::FramelessWindowHint);
    tabpane_->setAttribute(Qt::WA_TranslucentBackground);
    tabpane_->setStyleSheet(
        "QTabBar::tab { background: rgba(255, 255, 0, 128); } QTabWidget::pane "
        "{ border: "
        "1px solid lightgray; top:-1px; background:  transparent; }");

    //tabpane_->setStyleSheet("background: transparent; background-color: rgba(255, 255, 0, 128);");
    setCentralWidget(tabpane_);

    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(
        Qt::Window
        | Qt::FramelessWindowHint); //|Qt::WindowTransparentForInput|Qt::WindowStaysOnTopHint);

    setStyleSheet(
        "background: transparent; background-color: transparent;"); // + rgbaBackGroundStr_);
    // Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowStaysOnTopHint;
    // flags &= ~Qt::WindowTransparentForInput; // Опускаем последний бит
    // setWindowFlags(flags);
    // setWindowOpacity(0.6);
    setMouseTracking(true);

    // Central widget (tabpane_) covers the whole frameless window, so this
    // watches every mouse move to keep the border-resize cursor in sync.
    qApp->installEventFilter(this);

    // Dispatches Action mouse-chord/mixed aliases (Action::
    // get_mouse_bindings(), see widgets/controls/binding_dialogs.cpp) -
    // the mouse-side counterpart to Qt's native keyboard shortcut
    // dispatch. HeldButtonsTracker just observes; install it first so the
    // dispatcher can query held mouse buttons from a later keyPressEvent.
    qApp->installEventFilter(&HeldButtonsTracker::instance());
    qApp->installEventFilter(new ActionMouseDispatcher(this, this));
}


MainWindow::~MainWindow()
{
    delete tabpane_;
}


void MainWindow::quitProject()
{
    if (checkSave()) {
        exitProject();
    }
}


void MainWindow::saveAll()
{
    for (int i = tabpane_->count() - 1; i >= 0; --i) {
        tabpane_->setCurrentIndex(i);

        auto ret = fileactions_->saveFile();
        (void) ret;
    }
}


void MainWindow::newFile()
{
    fileactions_->newFile();
}


void MainWindow::settingsWindow()
{
    SettingsWindow* widget = new SettingsWindow(this, this->parentWidget());

    // widget->raise();
    widget->show();
    centered_widget(this, widget);
}

void MainWindow::settingsWindowNew()
{
    NewSettingsWindow* widget = new NewSettingsWindow(this,
                                                      this->parentWidget());

    widget->show();
    centered_widget(this, widget);
}


void MainWindow::saveFile()
{
    fileactions_->saveFile();
}


void MainWindow::quit()
{
    quitProject();
}

void MainWindow::openFile()
{
    fileactions_->openFile();
}


void MainWindow::saveFileAs()
{
    fileactions_->saveFileAs();
}

void MainWindow::settingsChangedSlot()
{
    auto* settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    backGroundColor_ = colorPreset[EPresetsColorIdx::kBackgroundColor];
    currentOpacity_ = settings->getCurrentOpacity();
    rgbaBackGroundStr_ = QString("rgba(%1, %2, %3, %4);")
                             .arg(backGroundColor_.red())
                             .arg(backGroundColor_.green())
                             .arg(backGroundColor_.blue())
                             .arg(currentOpacity_);

    tabpane_->setStyleSheet(
        "QTabBar::tab { background: rgba(255, 255, 0, 128); } QTabWidget::pane "
        "{ border: "
        "1px solid lightgray; top:-1px; background:  transparent; }");
    setStyleSheet(
        "background: transparent; background-color: transparent; "); // + rgbaBackGroundStr_);
    update();
}


bool MainWindow::checkSave()
{
    bool found = false;
    std::map<int, QString> items;
    int count = tabpane_->count();
    for (int i = 0; i < count; i++) {
        if (tabpane_->widgetAt(i)->isModified()) {
            found = true;
            items.emplace(i, tabpane_->widgetAt(i)->path());
        }
    }

    if (found) {
        QString details = "";
        for (auto& [_, p] : items) {
            details += p + "\n";
        }
        SaveAllWindow* widget = new SaveAllWindow(this, items);
        widget->setAttribute(Qt::WA_DeleteOnClose);
        widget->show();

        return false;
    }
    return true;
}

void MainWindow::saveAllWindowSaveCB(SaveAllWindow* w, std::map<int, bool>&& m)
{
    w->close();

    int exit_flag = 1;

    for (auto it = m.rbegin(); it != m.rend(); it++) {
        if (!it->second) {
            FLOG_DEBUG(Ch::UI,
                       "close ID = {} {}",
                       it->first,
                       tabpane_->getCurrentTabPath());
            tabpane_->closeTabByIndex(it->first);
        }
    }

    for (int i = tabpane_->count() - 1; i >= 0; --i) {
        FLOG_DEBUG(Ch::UI, "save ID = {} {}", i, tabpane_->getCurrentTabPath());
        tabpane_->setCurrentIndex(i);

        auto ret = fileactions_->saveFile();
        if (ret == QDialog::Rejected) {
            exit_flag = 0;
        } else {
            tabpane_->closeTabByIndex(i);
        }
    }

    if (exit_flag)
        exitProject();
}

void MainWindow::cleanupWorkplace()
{
    //    canvasWidget->cleanupWorkplace();
}

void MainWindow::exitProject()
{
    qApp->exit(0); // Is it correct way?
}

TabPane& MainWindow::tabPane()
{
    return *tabpane_;
}

FileActions& MainWindow::fileActions()
{
    return *fileactions_;
}

// ─── Window-level actions ─────────────────────────────────────────────────────
// Owned by MainWindow: a single QAction set for the whole app (ActionsMixin),
// since the menu bar/shortcuts are shared across all tabs.

// File
void MainWindow::on_action_new_scene()
{
    fileActions().newFile();
}

void MainWindow::on_action_open()
{
    fileActions().openFile();
}

void MainWindow::on_action_open_recent_file(const QString& filename)
{
    fileActions().processOpenFile(filename);
}

void MainWindow::on_action_quit()
{
    quitProject();
}

// View
void MainWindow::on_action_fullscreen(bool checked)
{
    if (checked)
        showFullScreen();
    else
        showNormal();
}

void MainWindow::on_action_always_on_top(bool checked)
{
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    // destroy()+create(), not hide()+show(): the window manager (X11's
    // WindowStaysOnTopHint maps to _NET_WM_STATE_ABOVE) can ignore a flag
    // change on an already-mapped window - hide/show remaps the same
    // native window handle, while destroy/create forces Qt to hand the
    // WM a brand new one with the current flags applied from scratch.
    destroy();
    create();
    show();
}

void MainWindow::on_action_show_menubar(bool checked)
{
    if (checked)
        setMenuBar(create_menubar());
    else
        setMenuBar(nullptr);
}

// Settings / Help
void MainWindow::on_action_settings()
{
    settingsWindow();
}

void MainWindow::on_action_settings_new()
{
    settingsWindowNew();
}

void MainWindow::on_action_keyboard_settings()
{
    // TODOLATER: open keyboard/mouse controls dialog
}

void MainWindow::on_action_open_settings_dir()
{
    QString dir = QFileInfo(SettingsHandler::getInstance()->settingsFileName())
                      .absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::on_action_help()
{
    // TODOLATER: open help dialog
}

void MainWindow::on_action_about()
{
    // Can't use the QMessageBox::about(...) convenience overload here: it
    // builds/execs/destroys the box internally, with no chance to apply
    // the fix below before it's shown. See FileActions::openFile() for
    // the same MainWindow-stylesheet-cascade issue on every other dialog
    // in this app.
    QMessageBox box(QMessageBox::NoIcon,
                    "About Familiar",
                    "<h2>Familiar</h2>"
                    "<p>Reference board application.</p>",
                    QMessageBox::Ok,
                    this);
    box.setIconPixmap(windowIcon().pixmap(64, 64));
    box.setAttribute(Qt::WA_TranslucentBackground, false);
    box.setStyleSheet("* { background-color: palette(window); color: "
                      "palette(window-text); }");
    box.exec();
}

void MainWindow::on_action_debuglog()
{
    // TODOLATER: open debug log dialog
}

// ─── Per-tab actions ───────────────────────────────────────────────────────────
// Thin forwarders to the currently active tab. Real logic lives on
// CanvasView. Guarded against a null currentWidget(): TabPane::onTabClosed()
// briefly has zero tabs while replacing the last closed one with a fresh
// untitled tab, which can surface here via Qt's synchronous signal delivery.

// File
void MainWindow::on_action_save()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_save();
}
void MainWindow::on_action_save_as()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_save_as();
}
void MainWindow::on_action_export_scene()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_export_scene();
}
void MainWindow::on_action_export_images()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_export_images();
}

// Edit
void MainWindow::on_action_undo()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_undo();
}
void MainWindow::on_action_redo()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_redo();
}
void MainWindow::on_action_select_all()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_select_all();
}
void MainWindow::on_action_deselect_all()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_deselect_all();
}
void MainWindow::on_action_cut()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_cut();
}
void MainWindow::on_action_copy()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_copy();
}
void MainWindow::on_action_paste()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_paste();
}
void MainWindow::on_action_delete_items()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_delete_items();
}
void MainWindow::on_action_raise_to_top()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_raise_to_top();
}
void MainWindow::on_action_lower_to_bottom()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_lower_to_bottom();
}

// View
void MainWindow::on_action_fit_scene()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_fit_scene();
}
void MainWindow::on_action_fit_selection()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_fit_selection();
}
void MainWindow::on_action_zoom_in()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_zoom_in();
}
void MainWindow::on_action_zoom_out()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_zoom_out();
}
// Insert
void MainWindow::on_action_insert_images()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_insert_images();
}
void MainWindow::on_action_insert_text()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_insert_text();
}

// Transform
void MainWindow::on_action_crop()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_crop();
}
void MainWindow::on_action_flip_horizontally()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_flip_horizontally();
}
void MainWindow::on_action_flip_vertically()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_flip_vertically();
}
void MainWindow::on_action_reset_scale()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_reset_scale();
}
void MainWindow::on_action_reset_rotation()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_reset_rotation();
}
void MainWindow::on_action_reset_flip()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_reset_flip();
}
void MainWindow::on_action_reset_crop()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_reset_crop();
}
void MainWindow::on_action_reset_transforms()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_reset_transforms();
}

// Normalize
void MainWindow::on_action_normalize_height()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_normalize_height();
}
void MainWindow::on_action_normalize_width()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_normalize_width();
}
void MainWindow::on_action_normalize_size()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_normalize_size();
}

// Arrange
void MainWindow::on_action_arrange_optimal()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_arrange_optimal();
}
void MainWindow::on_action_arrange_horizontal()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_arrange_horizontal();
}
void MainWindow::on_action_arrange_vertical()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_arrange_vertical();
}
void MainWindow::on_action_arrange_square()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_arrange_square();
}

// Images
void MainWindow::on_action_change_opacity()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_change_opacity();
}
void MainWindow::on_action_grayscale()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_grayscale();
}
void MainWindow::on_action_show_color_gamut()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_show_color_gamut();
}
void MainWindow::on_action_sample_color()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_sample_color();
}

// ─── Tab-switch action resync ─────────────────────────────────────────────────

void MainWindow::resyncActionsForTab(CanvasView* cv)
{
    // hookedScene_/hookedUndoStack_ are QPointer: if the previously-hooked
    // tab's scene/undoStack were already destroyed (this function can be
    // re-entered from inside the old tab's own ~CanvasView(), since Qt
    // emits currentChanged(-1) synchronously while QTabWidget removes the
    // about-to-be-destroyed widget), they've already nulled themselves
    // out and these disconnect() calls safely no-op instead of touching
    // freed memory.
    if (hookedScene_)
        disconnect(hookedScene_, nullptr, this, nullptr);
    if (hookedUndoStack_)
        disconnect(hookedUndoStack_, nullptr, this, nullptr);
    hookedScene_ = nullptr;
    hookedUndoStack_ = nullptr;

    if (!cv) {
        // Transient zero-tab moment (TabPane::onTabClosed replacing the
        // last closed tab); the very next currentTabChanged call, still
        // in the same call stack, corrects this.
        return;
    }

    hookedScene_ = cv->scene();
    hookedUndoStack_ = cv->undoStack();

    connect(hookedScene_,
            &CanvasScene::changed,
            this,
            &MainWindow::on_active_scene_changed);
    connect(hookedScene_,
            &CanvasScene::selectionChanged,
            this,
            &MainWindow::on_active_selection_changed);
    connect(hookedUndoStack_,
            &QUndoStack::canUndoChanged,
            this,
            &MainWindow::on_active_can_undo_changed);
    connect(hookedUndoStack_,
            &QUndoStack::canRedoChanged,
            this,
            &MainWindow::on_active_can_redo_changed);

    // Push current values immediately: the four signals above are
    // edge-triggered and don't replay the current state on connect.
    actiongroup_set_enabled("active_when_items_in_scene",
                            !cv->scene()->items().isEmpty());
    actiongroup_set_enabled("active_when_selection",
                            cv->scene()->has_selection());
    actiongroup_set_enabled("active_when_single_image",
                            cv->scene()->has_single_image_selection());
    actiongroup_set_enabled("active_when_can_undo", cv->undoStack()->canUndo());
    actiongroup_set_enabled("active_when_can_redo", cv->undoStack()->canRedo());
}

void MainWindow::onCurrentTabChanged(int index)
{
    resyncActionsForTab(tabpane_->widgetAt(index));
}

void MainWindow::on_active_scene_changed()
{
    actiongroup_set_enabled("active_when_items_in_scene",
                            !hookedScene_->items().isEmpty());
}

void MainWindow::on_active_selection_changed()
{
    actiongroup_set_enabled("active_when_selection",
                            hookedScene_->has_selection());
    actiongroup_set_enabled("active_when_single_image",
                            hookedScene_->has_single_image_selection());
}

void MainWindow::on_active_can_undo_changed(bool canUndo)
{
    actiongroup_set_enabled("active_when_can_undo", canUndo);
}

void MainWindow::on_active_can_redo_changed(bool canRedo)
{
    actiongroup_set_enabled("active_when_can_redo", canRedo);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (checkSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    // QEvent::ActivationChange is only delivered to the actual top-level
    // window, not to arbitrary descendants like CanvasView - push the
    // recompute down to every tab (not just the current one) so a tab
    // switched to later already has the right state, not just whichever
    // was visible when the window (de)activated.
    if (event->type() == QEvent::ActivationChange) {
        for (int i = 0; i < tabpane_->count(); ++i) {
            tabpane_->widgetAt(i)->updateSelectionVisibility();
        }
    }
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    qreal opacity = (qreal) currentOpacity_ / 255;
    painter.setOpacity(opacity);
    painter.fillRect(
        event->rect(),
        backGroundColor_); // тут поменяем цвет из настроек и сделаем доп функцию где будем менять опасити
    // Нарисуйте другие элементы интерфейса здесь
    //QMainWindow::paintEvent(event); // Вызов базовой реализации
}

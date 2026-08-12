#include "mainwindow.h"
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>

#include "canvasscene.h"
#include "project_settings.h"
#include "recovery.h"
#include "tabpane.h"
#include "widgets/about_dialog.h"
#include "widgets/dialogs.h"
#include "widgets/help_dialog.h"
#include "widgets/save_all_dialog.h"
#include <actions/action_mouse_dispatch.h>
#include <core/held_buttons_tracker.h>
#include <core/settings.h>
#include <core/settingshandler.h>
#include <map>
#include <ui/hierarchy_panel.h>
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
    // FIRST, before anything below can possibly create the native window
    // (e.g. fireInitialCheckableCallbacks_() inside
    // build_menu_and_actions() invokes window-management action slots):
    // an X11 window's ARGB visual is fixed at creation time, so if the
    // window is ever created before WA_TranslucentBackground is set,
    // transparency is silently dead for the whole session.
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    // tabpane_ is built here (constructor body), not via the
    // member-initializer list: building it constructs the first
    // CanvasView. Constructing it too early, before this class's own
    // setup is done, previously let Qt Designer's generated setupUi()
    // silently discard the real menu bar right after it was set (now
    // moot since setupUi() is gone, but the ordering still matters for
    // build_menu_and_actions() below).
    tabpane_ = new TabPane(this, *this);

    hierarchyPanel_ = new HierarchyPanel(this);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyPanel_);
    // Actual initial visibility comes from the persisted "hierarchy"
    // action state via fireInitialCheckableCallbacks_() below (same
    // mechanism show_menubar/auto_hide_ui/always_on_top already use) -
    // this hide() is just so it isn't briefly visible before that fires.
    hierarchyPanel_->hide();

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
    // Tab bar styling is set by settingsChangedSlot() above (line 65),
    // which already ran once with the initial settings - no need to
    // duplicate it here.
    setCentralWidget(tabpane_);
    // The menu bar already exists (built by the initial checkable
    // callbacks inside build_menu_and_actions() above); re-raise it above
    // the freshly-installed central widget.
    updateMenubarGeometry();

    // WA_TranslucentBackground + frameless flags are set at the very top
    // of this constructor (see comment there) - don't re-apply here:
    // setWindowFlags() with identical flags is a no-op, but keeping a
    // second copy invites the two call sites drifting apart.

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

    // Periodic autosave - see onAutosaveTimeout_()/
    // restartAutosaveTimer_() in mainwindow.h for the full picture.
    autosaveTimer_ = new QTimer(this);
    connect(autosaveTimer_,
            &QTimer::timeout,
            this,
            &MainWindow::onAutosaveTimeout_);
    connect(&SettingsEvents::instance(),
            &SettingsEvents::autosaveSettingsChanged,
            this,
            &MainWindow::restartAutosaveTimer_);
    restartAutosaveTimer_();

    // Crash recovery - see onRecoveryTimeout_() in
    // mainwindow.h. Always-on (unlike autosaveTimer_ above, not gated by
    // Save/autosave_enabled): it writes into its own recovery/ folder,
    // never the tab's real file, so there's no user-facing reason to
    // ever disable it.
    recoveryTimer_ = new QTimer(this);
    connect(recoveryTimer_,
            &QTimer::timeout,
            this,
            &MainWindow::onRecoveryTimeout_);
    recoveryTimer_->start(30000);

    // A clean exit means every tab's fate (saved or explicitly discarded)
    // has already been decided - nothing left in the recovery folder at
    // this point still needs recovering next launch. aboutToQuit() fires
    // exactly once on any normal shutdown path (qApp->exit() from
    // exitProject(), or the default quitOnLastWindowClosed after
    // closeEvent() accepts) - a real crash never reaches it, which is
    // exactly the point.
    connect(qApp, &QApplication::aboutToQuit, this, [] {
        familiar::recovery::clear();
    });

    // The startup recovery CHECK itself deliberately does NOT happen
    // here - see showOrOfferRecovery()'s comment (mainwindow.h): it has
    // to run before this window is ever shown, which main() controls,
    // not this constructor.
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

void MainWindow::onAutosaveTimeout_()
{
    // Unlike saveAll() above, this never touches tabpane_->setCurrentIndex()
    // - flipping the visibly-active tab every autosave tick would be a
    // jarring background interruption. FileActions::saveFile(CanvasView*,
    // path) exists specifically so this can save each tab in place.
    const int count = tabpane_->count();
    for (int i = 0; i < count; ++i) {
        CanvasView* cv = tabpane_->widgetAt(i);
        if (cv->isModified() && !cv->isUntitled()) {
            FLOG_INFO(Ch::IO, "Autosaving {}", cv->path().toStdString());
            fileactions_->saveFile(cv, cv->path());
        }
    }
}

void MainWindow::restartAutosaveTimer_()
{
    autosaveTimer_->stop();
    FamSettings settings;
    const bool enabled
        = settings.valueOrDefault(QStringLiteral("Save/autosave_enabled"))
              .toBool();
    const int seconds = settings
                            .valueOrDefault(QStringLiteral(
                                "Save/autosave_interval_seconds"))
                            .toInt();
    if (enabled)
        autosaveTimer_->start(seconds * 1000);
}

void MainWindow::onRecoveryTimeout_()
{
    const int count = tabpane_->count();
    for (int i = 0; i < count; ++i) {
        CanvasView* cv = tabpane_->widgetAt(i);
        // Unlike onAutosaveTimeout_() above, untitled tabs are NOT
        // skipped here - they're exactly the highest-risk data (never
        // written anywhere else) that this feature exists to protect.
        if (cv->isModified())
            familiar::recovery::save(cv);
    }
}

void MainWindow::showOrOfferRecovery(const QString& startupFile)
{
    const QList<familiar::recovery::Entry> entries = familiar::recovery::scan();

    if (entries.isEmpty()) {
        show();
        if (!startupFile.isEmpty())
            fileactions_->processOpenFile(startupFile);
        return;
    }
    // Parented to nullptr, not `this` - this window isn't shown yet, and
    // shouldn't be implied as this dialog's owner while it's invisible.
    auto* dlg = new RecoveryDialog(*fileactions_, entries, nullptr);
    // Both show() and the startupFile load wait for the dialog to
    // actually close now - previously processOpenFile(startupFile) ran
    // synchronously in main() right after this function returned, i.e.
    // BEFORE the (non-modal) dialog had a chance to process a Restore
    // click, so a recovery snapshot for that exact file raced its own
    // restore: whichever tab processOpenFile() had already claimed for
    // it was no longer "blank" by the time Restore ran, so restoring
    // landed in a SECOND, duplicate tab instead (Max). Deferring here
    // instead lets processOpenFile()'s own existing "already open ->
    // switch to that tab" dedup (see its top) do the right thing
    // either way: if the user restored it, that tab already exists and
    // this just switches to it; if they declined/unchecked it, no such
    // tab exists yet and this loads it fresh from disk exactly as
    // before.
    connect(dlg, &QObject::destroyed, this, [this, startupFile]() {
        show();
        if (!startupFile.isEmpty())
            fileactions_->processOpenFile(startupFile);
    });
}

void MainWindow::newFile()
{
    fileactions_->newFile();
}


void MainWindow::settingsWindow()
{
    SettingsWindow* widget = new SettingsWindow(this, this->parentWidget());

    widget->show();
    centered_widget(this, widget);
}

void MainWindow::openKeyboardShortcutsSettings()
{
    SettingsWindow* widget = new SettingsWindow(this, this->parentWidget());
    widget->selectCategory(tr("Keyboard Shortcuts"));
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

    const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
    // All tabs share the same background color; the selected one is
    // marked only by a selection-color underline. Unselected tabs get the
    // same 2px border in a transparent color rather than no border at
    // all, so the box model stays identical and tabs don't resize when
    // the selection changes.
    const QColor& selectionColor
        = colorPreset[EPresetsColorIdx::kSelectionColor];
    const QColor& borderColor = colorPreset[EPresetsColorIdx::kBorderColor];
    auto rgb = [](const QColor& c) {
        return QString("rgb(%1, %2, %3)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue());
    };
    // Same unstyled-tooltip-over-translucent-window issue as
    // updateWindowControlsStyle_(): without an explicit QToolTip rule,
    // hovering the tab close button shows a solid black plate. Tooltips
    // have no alpha channel - keep the color fully opaque.
    tabpane_->setStyleSheet(
        QString("QTabBar::tab { background: %1; color: %2; "
                "border-bottom: 2px solid transparent; "
                "padding: 6px 14px; } "
                "QTabBar::tab:selected { border-bottom: 2px solid %3; } "
                "QTabWidget::pane { border: 1px solid lightgray; top:-1px; "
                "background: transparent; } "
                "QToolTip { background-color: %1; color: %2; "
                "border: 1px solid %4; }")
            .arg(rgb(backGroundColor_),
                 rgb(textColor),
                 rgb(selectionColor),
                 rgb(borderColor)));
    setStyleSheet(
        "background: transparent; background-color: transparent; "); // + rgbaBackGroundStr_);
    updateWindowControlsStyle_();
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
        // Fire-and-forget, same as RecoveryDialog - SaveAllDialog shows
        // and self-deletes (WA_DeleteOnClose) on its own.
        new SaveAllDialog(this, items);
        return false;
    }
    return true;
}

void MainWindow::saveAllWindowSaveCB(SaveAllDialog* w, std::map<int, bool>&& m)
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
    // fireInitialCheckableCallbacks_() (action_mixin.h) invokes this at
    // startup, inside the constructor - showNormal()/showFullScreen()
    // would map the native window before WA_TranslucentBackground and
    // the frameless flags are set (they come later in the ctor), and an
    // X11 window keeps the non-ARGB visual it was created with forever:
    // transparency silently dies for the whole session. Only act on real
    // user toggles of an already-shown window.
    if (!isVisible())
        return;
    if (checked)
        showFullScreen();
    else
        showNormal();
}

void MainWindow::on_action_always_on_top(bool checked)
{
    // Same startup-callback guard as on_action_fullscreen() above - the
    // unconditional destroy()/create()/show() below used to run inside
    // the constructor (before the translucency attributes were set) and
    // permanently broke WA_TranslucentBackground for the session.
    if (windowFlags().testFlag(Qt::WindowStaysOnTopHint) == checked)
        return;
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

// ── Menu bar ─────────────────────────────────────────────────────────────────
// One long-lived QMenuBar laid at the top of the window (see mainwindow.h
// for why it is not installed via setMenuBar()). Two independent checkable
// actions drive it: show_menubar (exists/visible at all) and auto_hide_ui
// (fade the menu bar and the tab bar out when the cursor leaves the top
// strip, fade them back in when it returns). Both slots funnel into
// applyMenubarState_() so their initial firing order
// (fireInitialCheckableCallbacks_) is irrelevant.

void MainWindow::on_action_show_menubar(bool /*checked*/)
{
    applyMenubarState_();
}

void MainWindow::on_action_auto_hide_ui(bool /*checked*/)
{
    applyMenubarState_();
}

void MainWindow::on_action_hierarchy(bool checked)
{
    hierarchyPanel_->setVisible(checked);
}

void MainWindow::ensureMenubar_()
{
    if (menubar_)
        return;

    menubar_ = create_menubar();
    menubar_->setParent(this);

    // Window controls in the top-right corner. They live inside the menu
    // bar on purpose: with show_menubar off the app is a "clean" overlay
    // and the controls disappear together with it (actions/shortcuts keep
    // working).
    auto* corner = new QWidget(menubar_);
    windowControls_ = corner;
    auto* lay = new QHBoxLayout(corner);
    lay->setContentsMargins(4, 0, 4, 0);
    lay->setSpacing(2);
    auto makeButton = [corner, lay](const QString& glyph,
                                    const QString& tooltip) {
        auto* b = new QToolButton(corner);
        b->setText(glyph);
        b->setToolTip(tooltip);
        b->setAutoRaise(true);
        lay->addWidget(b);
        return b;
    };

    QToolButton* onTopBtn = makeButton("▲", "Always on top");
    QToolButton* minBtn = makeButton("–", "Minimize");
    QToolButton* maxBtn = makeButton("□", "Maximize / Restore");
    QToolButton* closeBtn = makeButton("✕", "Close");

    // Mirror the existing checkable always_on_top action instead of
    // duplicating its destroy()/create() logic - toggling either side
    // keeps the other in sync through the QAction.
    if (Action* a = getActions().find("always_on_top"); a && a->qaction) {
        onTopBtn->setCheckable(true);
        onTopBtn->setChecked(a->qaction->isChecked());
        connect(onTopBtn,
                &QToolButton::clicked,
                a->qaction,
                &QAction::setChecked);
        connect(a->qaction,
                &QAction::toggled,
                onTopBtn,
                &QToolButton::setChecked);
    }
    connect(minBtn, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(maxBtn, &QToolButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    // close() runs closeEvent() -> checkSave(), same as quitting from the
    // window manager.
    connect(closeBtn, &QToolButton::clicked, this, &MainWindow::close);

    menubar_->setCornerWidget(corner, Qt::TopRightCorner);
    updateWindowControlsStyle_();

    // Fade machinery - same QVariantAnimation pattern as the selection
    // outline fade in CanvasView. One animation drives both opacity
    // effects (menu bar + tab bar); the effects are only enabled while
    // hidden or animating, so fully-shown UI paints directly.
    menubarOpacity_ = new QGraphicsOpacityEffect(menubar_);
    menubarOpacity_->setOpacity(1.0);
    menubarOpacity_->setEnabled(false);
    menubar_->setGraphicsEffect(menubarOpacity_);

    tabbarOpacity_ = new QGraphicsOpacityEffect(tabpane_->tabBar());
    tabbarOpacity_->setOpacity(1.0);
    tabbarOpacity_->setEnabled(false);
    tabpane_->tabBar()->setGraphicsEffect(tabbarOpacity_);

    uiFadeAnim_ = new QVariantAnimation(this);
    connect(uiFadeAnim_,
            &QVariantAnimation::valueChanged,
            this,
            [this](const QVariant& value) {
                uiOpacity_ = value.toReal();
                menubarOpacity_->setOpacity(uiOpacity_);
                tabbarOpacity_->setOpacity(uiOpacity_);
                // The window's own background fill under the strip fades
                // too (paintEvent).
                update();
            });
    connect(uiFadeAnim_, &QVariantAnimation::finished, this, [this] {
        // Fully shown again - drop back to direct (effect-less) painting.
        if (uiFadeTargetVisible_) {
            menubarOpacity_->setEnabled(false);
            tabbarOpacity_->setEnabled(false);
        }
    });

    uiHideTimer_ = new QTimer(this);
    uiHideTimer_->setSingleShot(true);
    uiHideTimer_->setInterval(400);
    connect(uiHideTimer_, &QTimer::timeout, this, &MainWindow::onUiHideTimeout_);
}

void MainWindow::updateWindowControlsStyle_()
{
    if (!windowControls_)
        return;

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
    auto rgba = [](const QColor& c, int alpha) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(alpha);
    };

    // The window-wide "background: transparent" stylesheet strips
    // QToolButton's native hover/checked rendering, so spell the states
    // out explicitly. The QToolTip rule is here for the same reason the
    // context menu gets one in action_mixin.h: an unstyled tooltip over a
    // translucent window paints as a solid black plate. Tooltips have no
    // alpha channel - keep their colors fully opaque.
    windowControls_->setStyleSheet(
        QStringLiteral("QToolButton {"
                       "  background: transparent;"
                       "  color: %1;"
                       "  border: none;"
                       "  border-radius: 4px;"
                       "  padding: 2px 6px;"
                       "}"
                       "QToolButton:hover { background-color: %2; }"
                       "QToolButton:pressed { background-color: %3; }"
                       "QToolButton:checked { background-color: %3; }"
                       "QToolTip {"
                       "  background-color: %4;"
                       "  color: %1;"
                       "  border: 1px solid %5;"
                       "}")
            .arg(text.name(),
                 rgba(selection, 90),
                 rgba(selection, 170),
                 background.name(),
                 border.name()));
}

void MainWindow::applyMenubarState_()
{
    ensureMenubar_();

    Action* show = getActions().find("show_menubar");
    Action* autoHide = getActions().find("auto_hide_ui");
    const bool shown = show && show->qaction && show->qaction->isChecked();
    const bool wantAutoHide = autoHide && autoHide->qaction
                              && autoHide->qaction->isChecked();

    // Auto-hide is meaningless without a menu bar - grey it out.
    if (autoHide && autoHide->qaction)
        autoHide->qaction->setEnabled(shown);

    autoHideUi_ = shown && wantAutoHide;

    // Reset any in-flight fade to a clean, fully-opaque state.
    uiFadeAnim_->stop();
    uiHideTimer_->stop();
    menubarOpacity_->setEnabled(false);
    menubarOpacity_->setOpacity(1.0);
    tabbarOpacity_->setEnabled(false);
    tabbarOpacity_->setOpacity(1.0);
    uiFadeTargetVisible_ = true;
    uiOpacity_ = 1.0;

    menubar_->setVisible(shown);
    if (autoHideUi_)
        // Start visible, then fade away unless the cursor is over the
        // strip (the timeout re-checks).
        uiHideTimer_->start();

    updateMenubarGeometry();
    update();
}

void MainWindow::updateMenubarGeometry()
{
    if (!menubar_)
        return;
    const int h = menubar_->sizeHint().height();
    menubar_->setGeometry(0, 0, width(), h);
    // The central widget is (re)set after the initial applyMenubarState_()
    // call in the constructor, which would stack it above us.
    menubar_->raise();

    Action* show = getActions().find("show_menubar");
    const bool shown = show && show->qaction && show->qaction->isChecked();
    // The strip is reserved whenever the menu bar is enabled, auto-hide
    // included: the tab bar sits at the very top of the central widget,
    // i.e. exactly under the reveal zone - an overlaying bar would cover
    // the tabs the moment the cursor approaches them. With the strip
    // always reserved, auto-hide only blanks the strip visually and the
    // canvas/tabs never move or get covered.
    setContentsMargins(0, shown ? h : 0, 0, 0);
}

void MainWindow::startUiFade_(bool visible)
{
    uiFadeTargetVisible_ = visible;
    menubarOpacity_->setEnabled(true);
    tabbarOpacity_->setEnabled(true);
    uiFadeAnim_->stop();
    uiFadeAnim_->setStartValue(menubarOpacity_->opacity());
    uiFadeAnim_->setEndValue(visible ? 1.0 : 0.0);
    // Reveal must feel immediate; hiding matches the unhurried selection
    // outline fade (canvasview.cpp).
    uiFadeAnim_->setDuration(visible ? 200 : 600);
    uiFadeAnim_->start();
}

void MainWindow::onUiHideTimeout_()
{
    if (!autoHideUi_ || !uiFadeTargetVisible_)
        return;
    // Keep the UI while one of the menu bar's popups is open, or if the
    // cursor came back without generating a move (e.g. menu closed via
    // Esc) - re-arm and check again later.
    if (qApp->activePopupWidget()
        || uiStripContains_(mapFromGlobal(QCursor::pos()))) {
        uiHideTimer_->start();
        return;
    }
    startUiFade_(false);
}

void MainWindow::handleUiHover_(const QPoint& pos)
{
    if (!autoHideUi_)
        return;

    if (uiStripContains_(pos)) {
        uiHideTimer_->stop();
        if (!uiFadeTargetVisible_)
            startUiFade_(true);
    } else if (uiFadeTargetVisible_ && !uiHideTimer_->isActive()) {
        uiHideTimer_->start();
    }
}

bool MainWindow::tryStartWindowDrag_(const QPoint& pos)
{
    if (!windowHandle())
        return false;

    // Empty menu bar space drags the window; a menu title opens its
    // menu, the corner controls keep their clicks.
    if (menubar_ && menubar_->isVisible()) {
        const QPoint local = menubar_->mapFromParent(pos);
        if (menubar_->rect().contains(local)) {
            if (menubar_->actionAt(local) || menubar_->childAt(local))
                return false;
            windowHandle()->startSystemMove();
            return true;
        }
    }

    // Same for the tab-bar row: a click on a tab (or its close button)
    // switches/closes it, anywhere else in the row - including the empty
    // space right of the last tab, which belongs to the QTabWidget, not
    // the QTabBar - drags the window.
    if (QTabBar* tb = tabpane_ ? tabpane_->tabBar() : nullptr;
        tb && tb->isVisible()) {
        const QRect row(0,
                        tb->mapTo(this, QPoint(0, 0)).y(),
                        width(),
                        tb->height());
        if (row.contains(pos)) {
            const QPoint local = tb->mapFrom(this, pos);
            if (tb->rect().contains(local)
                && (tb->tabAt(local) >= 0 || tb->childAt(local)))
                return false;
            windowHandle()->startSystemMove();
            return true;
        }
    }
    return false;
}

bool MainWindow::uiStripContains_(const QPoint& pos) const
{
    return QRect(0, 0, width(), uiStripHeight_()).contains(pos);
}

int MainWindow::uiStripHeight_() const
{
    if (!menubar_)
        return 0;
    // Menu bar strip plus the tab bar right under it - the reveal zone,
    // the "don't hide yet" zone and the background region that fades
    // along with the widgets (paintEvent).
    int h = menubar_->sizeHint().height();
    if (QTabBar* tb = tabpane_ ? tabpane_->tabBar() : nullptr)
        h += tb->height();
    return h;
}

// Settings / Help
void MainWindow::on_action_settings()
{
    settingsWindow();
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
    new HelpDialog(this);
}

void MainWindow::on_action_about()
{
    new AboutDialog(this);
}

void MainWindow::on_action_debuglog()
{
    new DebugLogDialog(this);
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
void MainWindow::on_action_group()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_group();
}
void MainWindow::on_action_ungroup()
{
    if (auto* cv = tabpane_->currentWidget())
        cv->on_action_ungroup();
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
        hierarchyPanel_->setScene(nullptr, nullptr);
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
    // Hierarchy panel rebuild trigger. NOT CanvasScene::changed() (used
    // to be - see hierarchy_panel.h's own history comment): that signal
    // also fires continuously for as long as any GifItem is animating,
    // which starved a debounce forever and, even throttled, meant a
    // pointless rebuild every ~150ms just from a gif playing.
    // indexChanged only fires for real command-driven mutations -
    // animation doesn't push undo commands. The one gap this leaves
    // (background file loads, which bypass the undo stack via
    // add_queued_items()) is covered by an explicit
    // notifyStructuralChange() call from that call site instead.
    // Receiver is `this`, not hierarchyPanel_ directly: the disconnect()
    // calls at the top of this function only tear down connections whose
    // receiver is `this` (that's what makes switching tabs clean), so
    // routing through a MainWindow slot keeps that guarantee instead of
    // silently leaking a stale connection from the previous tab's
    // QUndoStack straight into the (still shared) hierarchy panel.
    connect(hookedUndoStack_,
            &QUndoStack::indexChanged,
            this,
            &MainWindow::notifyStructuralChange);

    hierarchyPanel_->setScene(cv->scene(), cv);

    // Push current values immediately: the four signals above are
    // edge-triggered and don't replay the current state on connect.
    actiongroup_set_enabled("active_when_items_in_scene",
                            !cv->scene()->items().isEmpty());
    actiongroup_set_enabled("active_when_selection",
                            cv->scene()->has_selection());
    actiongroup_set_enabled("active_when_single_image",
                            cv->scene()->has_single_image_selection());
    actiongroup_set_enabled("active_when_multi_selection",
                            cv->scene()->has_multi_selection());
    actiongroup_set_enabled("active_when_group_selected",
                            cv->scene()->has_group_selected());
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

void MainWindow::notifyStructuralChange()
{
    hierarchyPanel_->scheduleRefresh();
}

void MainWindow::on_active_selection_changed()
{
    actiongroup_set_enabled("active_when_selection",
                            hookedScene_->has_selection());
    actiongroup_set_enabled("active_when_single_image",
                            hookedScene_->has_single_image_selection());
    actiongroup_set_enabled("active_when_multi_selection",
                            hookedScene_->has_multi_selection());
    actiongroup_set_enabled("active_when_group_selected",
                            hookedScene_->has_group_selected());
    hierarchyPanel_->syncSelectionFromScene();
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

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateMenubarGeometry();
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    qreal opacity = (qreal) currentOpacity_ / 255;

    // In auto-hide-UI mode the window's own background fill in the top
    // strip (menu bar + tab bar) fades together with the widgets sitting
    // there - once hidden, the strip is fully transparent and only the
    // canvas remains visible. The strip still belongs to the window, so
    // it keeps receiving the hover events that reveal the UI again.
    const int stripH = autoHideUi_ ? uiStripHeight_() : 0;
    if (stripH > 0) {
        const QRect strip
            = QRect(0, 0, width(), stripH).intersected(event->rect());
        const QRect below = QRect(0, stripH, width(), height() - stripH)
                                .intersected(event->rect());
        if (!strip.isEmpty()) {
            painter.setOpacity(opacity * uiOpacity_);
            painter.fillRect(strip, backGroundColor_);
        }
        if (!below.isEmpty()) {
            painter.setOpacity(opacity);
            painter.fillRect(below, backGroundColor_);
        }
        return;
    }

    painter.setOpacity(opacity);
    painter.fillRect(
        event->rect(),
        backGroundColor_); // тут поменяем цвет из настроек и сделаем доп функцию где будем менять опасити
    // Нарисуйте другие элементы интерфейса здесь
    //QMainWindow::paintEvent(event); // Вызов базовой реализации
}

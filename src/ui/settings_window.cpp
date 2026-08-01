#include "settings_window.h"
#include "mainwindow.h"
#include <core/controls.h>
#include <core/settings.h>
#include <ui/colors_widget.h>
#include <widgets/controls/keyboard_controls.h>
#include <widgets/controls/mouse_controls.h>
#include <widgets/controls/mousewheel_controls.h>
#include <widgets/dialogs.h>
#include <widgets/settings_dialog.h>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTabBar>

SettingsWindow::SettingsWindow(MainWindow* wm, QWidget* parent)
    : QWidget(parent)
    , window_(wm)
    , tabWidget_(new QTabWidget(this))
    , miscTab_(new QWidget)
    , confirmCloseUnsaved_(new ConfirmCloseUnsavedWidget)
    , imagesTab_(new QWidget)
    , imageStorageFormat_(new ImageStorageFormatWidget)
    , arrangeGap_(new ArrangeGapWidget)
    , allocationLimit_(new AllocationLimitWidget)
    , arrangeDefault_(new ArrangeDefaultWidget)
    , colorsTab_(new QWidget)
    , colors_(new ColorsWidget(this))
    , shortcutsTab_(new QWidget)
    , shortcuts_(new KeyboardShortcutsView)
    , mouseTab_(new QWidget)
    , mouse_(new MouseView)
    , mouseWheelTab_(new QWidget)
    , mouseWheel_(new MouseWheelView)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint);
    // Same as NewSettingsWindow: not widget-parented to MainWindow (its
    // transparent stylesheet would cascade in), so with Always On Top
    // enabled the main window would cover this modal window - inherit
    // the hint explicitly.
    if (wm && wm->windowFlags().testFlag(Qt::WindowStaysOnTopHint))
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowModality(Qt::ApplicationModal);
    // We wrap QTabWidget in a QWidget because of a Qt bug
    auto* layout = new QVBoxLayout(this);
    tabWidget_->tabBar()->setUsesScrollButtons(false);
    layout->addWidget(tabWidget_);

    resize(640, this->geometry().height());
    setFixedSize(640, this->geometry().height());

    setWindowTitle(tr("Configuration"));

    // Miscellaneous
    auto* miscLayout = new QGridLayout(miscTab_);
    miscLayout->addWidget(confirmCloseUnsaved_, 0, 0);
    tabWidget_->addTab(miscTab_, tr("Miscellaneous"));

    // Images & Items
    auto* imagesLayout = new QGridLayout(imagesTab_);
    imagesLayout->addWidget(imageStorageFormat_, 0, 0);
    imagesLayout->addWidget(allocationLimit_, 0, 1);
    imagesLayout->addWidget(arrangeGap_, 1, 0);
    imagesLayout->addWidget(arrangeDefault_, 1, 1);
    tabWidget_->addTab(imagesTab_, tr("Images && Items"));

    // Colors
    auto* colorsLayout = new QVBoxLayout(colorsTab_);
    colorsLayout->addWidget(colors_);
    tabWidget_->addTab(colorsTab_, tr("Colors"));

    // Shortcuts
    auto* shortcutsLayout = new QVBoxLayout(shortcutsTab_);
    shortcutsLayout->addWidget(shortcuts_);
    tabWidget_->addTab(shortcutsTab_, tr("Shortcuts"));

    // Mouse
    auto* mouseLayout = new QVBoxLayout(mouseTab_);
    mouseLayout->addWidget(mouse_);
    tabWidget_->addTab(mouseTab_, tr("Mouse"));

    // Mouse Wheel
    auto* mouseWheelLayout = new QVBoxLayout(mouseWheelTab_);
    mouseWheelLayout->addWidget(mouseWheel_);
    tabWidget_->addTab(mouseWheelTab_, tr("Mouse Wheel"));

    // Resets every tab's settings back to defaults - Miscellaneous/Images
    // & Items (FamSettings, carried over from the previously-unwired
    // SettingsDialog in widgets/settings_dialog.cpp) as well as
    // Shortcuts/Mouse/Mouse Wheel (KeyboardSettings - same underlying
    // QSettings store despite the class name, see core/controls.h).
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QWidget::close);

    auto* resetBtn = new QPushButton(tr("Restore Defaults"), this);
    resetBtn->setAutoDefault(false);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto reply = showMessageBox(
            QMessageBox::Question,
            this,
            tr("Restore defaults?"),
            tr("Do you want to restore all settings to their default values?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            FamSettings().restoreDefaults();
            KeyboardSettings().restoreDefaults();
        }
    });
    buttons->addButton(resetBtn, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
}

void SettingsWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}

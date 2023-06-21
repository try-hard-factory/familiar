#include "settings_window.h"
#include "mainwindow.h"
#include <core/settingshandler.h>
#include <ui/colors_widget.h>
#include <ui/preferencesconf.h>
#include <ui/shortcuts_widget.h>
#include <QTabBar>

SettingsWindow::SettingsWindow(MainWindow* wm, QWidget* parent)
    : QWidget(parent)
    , window_(wm)
    , tabWidget_(new QTabWidget(this))
    , prefConfigTab_(new QWidget)
    , prefConfig_(new PreferencesConf)
    , colorsTab_(new QWidget)
    , colors_(new ColorsWidget(this))
    , shortcutsTab_(new QWidget)
    , shortcuts_(new ShortcutsWidget)
{
    // We wrap QTabWidget in a QWidget because of a Qt bug
    auto* layout = new QVBoxLayout(this);
    tabWidget_->tabBar()->setUsesScrollButtons(false);
    layout->addWidget(tabWidget_);

    resize(640, this->geometry().height());    

    setAttribute(Qt::WA_DeleteOnClose);

    setFixedSize(640, this->geometry().height());

    //    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Configuration"));

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::fileChanged,
            this,
            &SettingsWindow::updateChildren);
    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            window_,
            &MainWindow::settingsChangedSlot);
            

    QColor background = this->palette().window().color();
    //    bool isDark = ColorUtils::colorIsDark(background);
    //    QString modifier =
    //      isDark ? PathInfo::whiteIconPath() : PathInfo::blackIconPath();

    // general
    auto* prefConfigLayout = new QVBoxLayout(prefConfigTab_);
    prefConfigTab_->setLayout(prefConfigLayout);
    prefConfigLayout->addWidget(prefConfig_);
    tabWidget_->addTab(prefConfigTab_, tr("Preferences"));

    // colors
    auto* colorsLayout = new QVBoxLayout(colorsTab_);
    colorsTab_->setLayout(colorsLayout);
    colorsLayout->addWidget(colors_);
    tabWidget_->addTab(colorsTab_, tr("Colors"));

    // shortcuts
    auto* shortcutsLayout = new QVBoxLayout(shortcutsTab_);
    shortcutsTab_->setLayout(shortcutsLayout);
    shortcutsLayout->addWidget(shortcuts_);
    tabWidget_->addTab(shortcutsTab_, tr("Shortcuts"));

    connect(this,
            &SettingsWindow::updateChildren,
            prefConfig_,
            &PreferencesConf::updateComponents);

    setWindowFlags(Qt::Window |  Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint);
}

void SettingsWindow::slidertest_out()
{
    qDebug() << SettingsHandler::getInstance()->masterOpacity() << " SettingsWindow";
}

void SettingsWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}

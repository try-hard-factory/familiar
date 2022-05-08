#include "settings_window.h"
#include "mainwindow.h"
#include <core/settingshandler.h>
#include <QTabBar>
#include <ui/preferencesconf.h>
#include <ui/colors_widget.h>
#include <ui/shortcuts_widget.h>

SettingsWindow::SettingsWindow(MainWindow *wm, QWidget *parent)
    : QWidget(parent), window_(wm)
{
    // We wrap QTabWidget in a QWidget because of a Qt bug
    auto* layout = new QVBoxLayout(this);
    tabWidget_ = new QTabWidget(this);
    tabWidget_->tabBar()->setUsesScrollButtons(false);
    layout->addWidget(tabWidget_);

    resize(640, this->geometry().height());

    setAttribute(Qt::WA_DeleteOnClose);
//    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Configuration"));

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::fileChanged,
            this,
            &SettingsWindow::updateChildren);

    QColor background = this->palette().window().color();
//    bool isDark = ColorUtils::colorIsDark(background);
//    QString modifier =
//      isDark ? PathInfo::whiteIconPath() : PathInfo::blackIconPath();

    // general
    prefConfig_ = new PreferencesConf();
    prefConfigTab_ = new QWidget();
    auto* prefConfigLayout = new QVBoxLayout(prefConfigTab_);
    prefConfigTab_->setLayout(prefConfigLayout);
    prefConfigLayout->addWidget(prefConfig_);
    tabWidget_->addTab(prefConfigTab_, tr("Preferences"));

    // colors
    colors_ = new ColorsWidget();
    colorsTab_ = new QWidget();
    auto* colorsLayout = new QVBoxLayout(colorsTab_);
    colorsTab_->setLayout(colorsLayout);
    colorsLayout->addWidget(colors_);
    tabWidget_->addTab(colorsTab_, tr("Colors"));

    // shortcuts
    shortcuts_ = new ShortcutsWidget();
    shortcutsTab_ = new QWidget();
    auto* shortcutsLayout = new QVBoxLayout(shortcutsTab_);
    shortcutsTab_->setLayout(shortcutsLayout);
    shortcutsLayout->addWidget(shortcuts_);
    tabWidget_->addTab(shortcutsTab_, tr("Shortcuts"));

    connect(this,
            &SettingsWindow::updateChildren,
            prefConfig_,
            &PreferencesConf::updateComponents);
}

void SettingsWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}

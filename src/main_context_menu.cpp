#include "main_context_menu.h"
#include "mainwindow.h"

MainContextMenu::MainContextMenu(MainWindow& wm, QWidget* parent)
    : QMenu(parent)
    , mainwindow_(wm)
{
    quitAction_ = new QAction("quit", this);
    connect(quitAction_, SIGNAL(triggered()), this, SLOT(quit()));
    addAction(quitAction_);

    settingsAction_ = new QAction("settings", this);
    connect(settingsAction_, SIGNAL(triggered()), this, SLOT(settings()));
    addAction(settingsAction_);

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &MainContextMenu::settingsChangedSlot);

    settingsChangedSlot();
}

MainContextMenu::~MainContextMenu()
{
    delete quitAction_;
}


void MainContextMenu::settingsChangedSlot()
{
    auto settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    menuColor_ = colorPreset[EPresetsColorIdx::kMenuColor];
    //fileMenu_->setStyleSheet("background: transparent; background-color: rgba(0, 255, 0, 255);");
    QString rgbaColor = QString("rgba(%1, %2, %3, %4);")
                            .arg(menuColor_.red())
                            .arg(menuColor_.green())
                            .arg(menuColor_.blue())
                            .arg(255);
    setStyleSheet("background: transparent; background-color: " + rgbaColor);
}

void MainContextMenu::quit()
{
    mainwindow_.quitProject();
}


void MainContextMenu::settings()
{
    mainwindow_.settingsWindow();
}

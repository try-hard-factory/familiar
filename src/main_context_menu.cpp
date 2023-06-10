#include "main_context_menu.h"
#include "mainwindow.h"

MainContextMenu::MainContextMenu(MainWindow& wm, QWidget* parent)
    : QMenu(parent)
    , mainwindow_(wm)
{
    quit_action = new QAction("quit", this);
    connect(quit_action, SIGNAL(triggered()), this, SLOT(quit()));
    addAction(quit_action);
}

MainContextMenu::~MainContextMenu()
{
    delete quit_action;
}

void MainContextMenu::quit()
{
    mainwindow_.quitProject();
}

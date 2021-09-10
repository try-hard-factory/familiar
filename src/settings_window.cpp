#include "settings_window.h"
#include "mainwindow.h"

SettingsWindow::SettingsWindow(MainWindow *wm, QWidget *parent)
    : QWidget(parent), window_(wm)
{
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::Tool | Qt::Dialog);
    setWindowTitle("settings");
    resize(320, 240);

    centered_widget(window_, this);
}

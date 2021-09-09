#include "settings_window.h"
#include "mainwindow.h"

SettingsWindow::SettingsWindow(MainWindow *wm, QWidget *parent)
    : QWidget(parent), window_(wm)
{
    centered_widget(window_, this);
}

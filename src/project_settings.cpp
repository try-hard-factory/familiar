#include "project_settings.h"
#include "mainwindow.h"

project_settings::project_settings(MainWindow *mw)
    : mw_(mw)
{
    mw_->setWindowTitle(title());
}

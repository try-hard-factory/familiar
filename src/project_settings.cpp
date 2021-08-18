#include "project_settings.h"
#include "mainwindow.h"

project_settings::project_settings(MainWindow *mw)
    : mw_(mw)
{
    mw_->setWindowTitle(title());
}

void project_settings::title(QString t)
{
    title_ = t;
    mw_->setWindowTitle(title_);
}

void project_settings::path(QString p)
{
    path_ = p;
}

void project_settings::modified(bool s)
{
    changed_ = s;
    if (changed_ == true) {
        mw_->setWindowTitle("*"+title());
    }
}

#include "project_settings.h"
#include "mainwindow.h"
#include <QFileInfo>

project_settings::project_settings(TabPane* tp)
    : tp_(tp)
{
    //    mw_->setWindowTitle(title());
}

void project_settings::title(const QString& t)
{
    title_ = t;
    tp_->setCurrentTabTitle(title_);
}

void project_settings::path(const QString& p)
{
    path_ = p;
}

void project_settings::projectName(const QString& p)
{
    projectName_ = p;
}

void project_settings::modified(bool s)
{
    changed_ = s;
    if (changed_ == true) {
        tp_->setCurrentTabTitle("*" + QFileInfo(path_).fileName());
    } else {
        tp_->setCurrentTabTitle(QFileInfo(path_).fileName());
    }
}

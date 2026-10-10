#include "project_settings.h"
#include "mainwindow.h"
#include <QFileInfo>

ProjectSettings::ProjectSettings(TabPane* tp, CanvasView* view)
    : tp_(tp)
    , view_(view)
{
    //    mw_->setWindowTitle(title());
}

void ProjectSettings::title(const QString& t)
{
    title_ = t;
    tp_->set_tab_title(view_, title_);
}

void ProjectSettings::path(const QString& p)
{
    path_ = p;
}

void ProjectSettings::project_name(const QString& p)
{
    projectName_ = p;
}

void ProjectSettings::modified(bool s)
{
    changed_ = s;
    if (changed_ == true) {
        tp_->set_tab_title(view_, "*" + QFileInfo(path_).fileName());
    } else {
        tp_->set_tab_title(view_, QFileInfo(path_).fileName());
    }
}

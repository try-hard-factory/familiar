#include "recent_files_view.h"

#include "mainwindow.h"

void RecentFilesView::on_clicked(const QModelIndex& index)
{
    if (!mainWindow_ || !index.isValid() || index.row() >= files_.size()) {
        return;
    }
    mainWindow_->fileActions().processOpenFile(files_[index.row()]);
}

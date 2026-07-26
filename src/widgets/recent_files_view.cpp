#include "recent_files_view.h"

#include "mainwindow.h"

void RecentFilesView::on_clicked(const QModelIndex& index)
{
    if (!mainWindow_ || !index.isValid() || index.row() >= files.size()) {
        return;
    }
    mainWindow_->fileActions().processOpenFile(files[index.row()]);
}

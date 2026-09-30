#include "recent_files_view.h"

#include "mainwindow.h"

void RecentFilesView::on_clicked(const QModelIndex& index)
{
    if (!mainWindow_ || !index.isValid() || index.row() >= files_.size()) {
        return;
    }
    mainWindow_->file_actions().process_open_file(files_[index.row()]);
}

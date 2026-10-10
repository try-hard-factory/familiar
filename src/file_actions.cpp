#include "file_actions.h"
#include "canvasscene.h"
#include "fileio.h"
#include "fml_archive.h"
#include "mainwindow.h"
#include "recovery.h"
#include "tabpane.h"
#include "widgets/dialogs.h"
#include "widgets/file_browser_dialog.h"
#include <canvasview.h>
#include <core/settingshandler.h>
#include <QMessageBox>
#include <QString>

FileActions::FileActions(MainWindow& mw)
    : mainwindow_(mw)
{}

FileActions::~FileActions() {}

void FileActions::new_file()
{
    mainwindow_.tab_pane().add_new_untitled_tab();
}

void FileActions::open_file()
{
    // SVG/Adobe filter entries used to be listed here too (pre-existing,
    // not something this refactor added) but loadFmlIntoCurrentTab()
    // only ever knows how to parse the .fml zip+manifest archive - a
    // raw .svg/.psd file just fails to open as one, so those filters
    // were pure misdirection.
    const QStringList files = show_open_files_dialog(&mainwindow_,
                                                     QObject::tr("Open"),
                                                     QDir::homePath(),
                                                     QStringLiteral(
                                                         "Familiar (*.fml)"));
    for (const QString& file : files) {
        process_open_file(file);
    }
}

void FileActions::load_fml_into_current_tab(const QString& path,
                                            bool markModifiedAfterLoad,
                                            const QUuid& recoveryIdToClear)
{
    CanvasView* canvasView = mainwindow_.tab_pane().current_widget();
    CanvasScene* scene = canvasView->scene();

    auto* worker = new ThreadedIO(
        [path, scene](ThreadedIO* w) { load_fml(path, scene, w); });

    QObject::connect(
        worker,
        &ThreadedIO::finished,
        &mainwindow_,
        [this,
         canvasView,
         scene,
         markModifiedAfterLoad,
         recoveryIdToClear](const QString& error,
                            const QStringList& itemErrors) {
            scene->add_queued_items();
            // add_queued_items() populates the scene directly, bypassing
            // the undo stack entirely - the Hierarchy panel's rebuild
            // trigger is QUndoStack::indexChanged (see
            // resyncActionsForTab()), which won't fire for this, so it
            // needs an explicit nudge here.
            mainwindow_.notify_structural_change();
            // Before fit_scene(): seeds canvasRect_ from what
            // FmlArchive::load() stashed on the scene, so fit_scene()
            // has something to fall back to even if this project was
            // saved with zero items (see CanvasScene::
            // remembered_bounding_rect()).
            canvasView->restore_canvas_rect(scene->remembered_bounding_rect());
            canvasView->on_action_fit_scene();
            canvasView->set_modified(markModifiedAfterLoad);

            // Only now - after the background read has actually
            // succeeded - is it safe to delete the recovery file it was
            // just read from. Also only on success: if loading failed,
            // keep the recovery snapshot around rather than silently
            // losing the only copy of that data.
            if (error.isEmpty() && !recoveryIdToClear.isNull()) {
                familiar::recovery::remove(recoveryIdToClear);
            }

            if (!error.isEmpty()) {
                show_message_box(QMessageBox::Critical,
                                 &mainwindow_,
                                 QObject::tr("Could not open file"),
                                 error);
            }
            if (!itemErrors.isEmpty()) {
                QStringList lines;
                for (const QString& e : itemErrors) {
                    lines.append(QStringLiteral("<li>%1</li>").arg(e));
                }
                show_message_box(
                    QMessageBox::Warning,
                    &mainwindow_,
                    QObject::tr("Problem loading project"),
                    QObject::tr("%1 item(s) could not be loaded.<ul>%2</ul>")
                        .arg(itemErrors.size())
                        .arg(lines.join(QString())));
            }
        });

    QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    new ProgressDialog(QObject::tr("Opening project"), worker, 0, &mainwindow_);
    worker->start();
}

void FileActions::restore_from_recovery(const QString& recoveryFmlPath,
                                        const QString& originalPath,
                                        const QUuid& recoveryId)
{
    if (originalPath.isEmpty()) {
        mainwindow_.tab_pane().add_new_untitled_tab();
    } else {
        mainwindow_.tab_pane().add_new_tab(originalPath);
    }
    load_fml_into_current_tab(recoveryFmlPath,
                              /*markModifiedAfterLoad=*/true,
                              recoveryId);
}

CanvasView* FileActions::find_blank_tab()
{
    TabPane& tp = mainwindow_.tab_pane();
    for (int i = 0; i < tp.count(); ++i) {
        CanvasView* cv = tp.widget_at(i);
        if (cv->is_untitled() && !cv->is_modified()) {
            return cv;
        }
    }
    return nullptr;
}

void FileActions::close_tab(CanvasView* cv)
{
    // Same direct `delete` TabPane::onTabClosed() uses for its own
    // "nothing to lose" branch - Qt's QTabWidget notices the child
    // widget being destroyed and removes its tab entry on its own, no
    // separate closeTabByIndex() call needed.
    delete cv;
}

void FileActions::process_open_file(const QString& file)
{
    // Already open in some tab - switch to it instead of opening a
    // second copy. Was previously only checked in openFile()'s dialog
    // loop, so on_action_open_recent_file() (which calls this directly)
    // skipped it entirely.
    const int count = mainwindow_.tab_pane().count();
    for (int j = 0; j < count; ++j) {
        if (mainwindow_.tab_pane().widget_at(j)->path() == file) {
            mainwindow_.tab_pane().set_current_index(j);
            return;
        }
    }

    if (mainwindow_.tab_pane().current_widget()->is_untitled()
        && mainwindow_.tab_pane().current_widget()->is_modified() == false) {
        mainwindow_.tab_pane().set_current_tab_path(file);
        mainwindow_.tab_pane().set_current_tab_title(QFileInfo(file).fileName());
        mainwindow_.tab_pane().set_current_tab_project_name(
            QFileInfo(file).fileName());
    } else {
        mainwindow_.tab_pane().add_new_tab(file);
    }

    load_fml_into_current_tab(file);

    SettingsHandler::update_recent_files(file);
    mainwindow_.update_menu_and_actions();
}

int FileActions::save_file(CanvasView* canvasView, const QString& path)
{
    const QFile file(path);
    if (!file.exists()) {
        return save_file_as();
    }

    // Synchronous (not backgrounded like loadFmlIntoCurrentTab()): several
    // callers (TabPane::onTabClosed(), MainWindow::saveAllWindowSaveCB())
    // close the tab or quit the app right after this returns, assuming the
    // save has already completed - threading it would need those flows
    // reworked to wait on ThreadedIO::finished first.
    const FmlResult result = FmlArchive::save(canvasView->scene(),
                                              canvasView->canvas_rect(),
                                              path);

    if (!result.error.isEmpty()) {
        show_message_box(QMessageBox::Critical,
                         &mainwindow_,
                         QObject::tr("Could not save file"),
                         result.error);
        return QDialog::Rejected;
    }

    if (!result.itemErrors.isEmpty()) {
        QStringList lines;
        for (const QString& e : result.itemErrors) {
            lines.append(QStringLiteral("<li>%1</li>").arg(e));
        }
        show_message_box(QMessageBox::Warning,
                         &mainwindow_,
                         QObject::tr("Problem saving project"),
                         QObject::tr(
                             "%1 item(s) could not be saved.<ul>%2</ul>")
                             .arg(result.itemErrors.size())
                             .arg(lines.join(QString())));
    }

    // Marks the undo stack's current position as the new "saved"
    // baseline; CanvasView::on_undo_clean_changed() reacts to moving
    // away from it by calling setModified(true). Without this, nothing
    // ever un-marks the project as modified after a save (setModified(
    // false) below is a redundant belt-and-suspenders default; the
    // clean-index tracking is what actually stays correct across
    // undo/redo).
    canvasView->undo_stack()->setClean();
    canvasView->set_modified(false);

    // See processOpenFile()'s comment - saveFileAs() reaches this via
    // its own call to saveFile(selected), so this covers both.
    SettingsHandler::update_recent_files(path);
    mainwindow_.update_menu_and_actions();

    return QDialog::Accepted;
}

int FileActions::save_file(const QString& path)
{
    return save_file(mainwindow_.tab_pane().current_widget(), path);
}

int FileActions::save_file()
{
    return save_file(mainwindow_.tab_pane().current_widget()->path());
}

int FileActions::save_file_as()
{
    int retval = QDialog::Rejected;
    // showSaveFileDialog() already appends the right extension for
    // whichever filter is selected (same job fileExt_ used to do here
    // manually) and confirms overwrite internally. SVG/Adobe filter
    // entries removed - saveFile()/FmlArchive only ever write the .fml
    // zip+manifest format, there's no actual SVG/PSD export path here.
    const QString selected = show_save_file_dialog(&mainwindow_,
                                                   QObject::tr("Save As"),
                                                   QDir::homePath(),
                                                   QStringLiteral(
                                                       "Familiar (*.fml)"));

    if (!selected.isEmpty()) {
        if (!mainwindow_.tab_pane().current_widget()->is_untitled()
            && mainwindow_.tab_pane().get_current_tab_project_name()
                   != QFileInfo(selected).fileName()) {
            auto* canvasView = mainwindow_.tab_pane().current_widget();
            mainwindow_.tab_pane().add_new_tab(selected);
            load_fml_into_current_tab(canvasView->path());
        }

        mainwindow_.tab_pane().set_current_tab_path(selected);
        mainwindow_.tab_pane().set_current_tab_title(
            QFileInfo(selected).fileName());
        mainwindow_.tab_pane().set_current_tab_project_name(
            QFileInfo(selected).fileName());

        // Pre-touch the file into existence: saveFile(path) bails into
        // saveFileAs() again if the path doesn't exist yet, which would
        // recurse right back here for a genuinely new file.
        (void) QFile(selected).open(QFile::ReadWrite);
        save_file(selected);
        retval = QDialog::Accepted;
    }

    return retval;
}

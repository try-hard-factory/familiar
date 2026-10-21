#include "file_actions.h"
#include "canvasscene.h"
#include "fileio.h"
#include "fml_archive.h"
#include "log/log.h"
#include "mainwindow.h"
#include "recovery.h"
#include "tabpane.h"
#include "widgets/dialogs.h"
#include "widgets/file_browser_dialog.h"
#include "widgets/message_box.h"
#include <canvasview.h>
#include <core/settingshandler.h>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QObject>
#include <QString>
#include <QThread>
#include <QUuid>
#include <QtContainerFwd>

FileActions::FileActions(MainWindow& mw)
    : mainwindow_(mw)
{}

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
                                            const QUuid& recoveryIdToClear,
                                            TabOwnership tabOwnership)
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
         recoveryIdToClear,
         tabOwnership](const QString& error, const QStringList& itemErrors) {
            // A fatal error means nothing was queued and the tab is
            // empty, so the whole success path below (fit_scene, the
            // modified flag, deleting the recovery snapshot) has nothing
            // to act on. Handled first, and the tab is not left behind
            // as an empty page titled after a file it never managed to
            // open.
            if (!error.isEmpty()) {
                // Tab first, dialog second: show_message_box() is modal
                // and blocks here until dismissed, so doing it the other
                // way round leaves the half-opened tab sitting behind the
                // error for as long as the user takes to read it.
                discard_failed_tab(canvasView, tabOwnership);
                show_message_box(QMessageBox::Critical,
                                 &mainwindow_,
                                 QObject::tr("Could not open file"),
                                 error);
                return;
            }

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
            // just read from. The failure path above returns before this
            // instead, keeping the snapshot rather than silently losing
            // the only copy of that data.
            if (!recoveryIdToClear.isNull()) {
                familiar::recovery::remove(recoveryIdToClear);
            }

            // Per-item problems are NOT a failed open: whatever did load
            // is real content the user should keep, so this tab stays.
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
                              recoveryId,
                              TabOwnership::CreatedForThisLoad);
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

void FileActions::discard_failed_tab(CanvasView* canvasView,
                                     TabOwnership tabOwnership)
{
    FLOG_DEBUG(familiar::log::Ch::IO,
               "Open failed - {} its tab",
               tabOwnership == TabOwnership::CreatedForThisLoad
                   ? "closing"
                   : "reverting");
    if (tabOwnership == TabOwnership::CreatedForThisLoad) {
        close_tab(canvasView);
        // close_tab() is a plain delete and QTabWidget drops the page on
        // its own, but nothing refills an emptied pane - TabPane::
        // on_tab_closed() does this same check for the same reason.
        if (mainwindow_.tab_pane().count() == 0) {
            mainwindow_.tab_pane().add_new_untitled_tab();
        }
        return;
    }

    // The tab existed before this load (process_open_file() reuses an
    // already-blank one instead of stacking a second empty tab next to
    // it), so it is the user's, not ours to close: put its identity back
    // the way it was. Addressed through the captured view rather than
    // current_widget(), since the load is backgrounded and the user may
    // have switched tabs while it ran. "untitled" is the sentinel
    // ProjectSettings itself uses for all three (project_settings.h).
    const QString untitled = QStringLiteral("untitled");
    canvasView->set_path(untitled);
    canvasView->set_project_name(untitled);
    mainwindow_.tab_pane().set_tab_title(canvasView, untitled);
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

    bool reusedBlankTab = false;
    if (mainwindow_.tab_pane().current_widget()->is_untitled()
        && mainwindow_.tab_pane().current_widget()->is_modified() == false) {
        mainwindow_.tab_pane().set_current_tab_path(file);
        mainwindow_.tab_pane().set_current_tab_title(QFileInfo(file).fileName());
        mainwindow_.tab_pane().set_current_tab_project_name(
            QFileInfo(file).fileName());
        reusedBlankTab = true;
    } else {
        mainwindow_.tab_pane().add_new_tab(file);
    }

    load_fml_into_current_tab(file,
                              /*markModifiedAfterLoad=*/false,
                              /*recoveryIdToClear=*/QUuid(),
                              reusedBlankTab
                                  ? TabOwnership::ExistedBefore
                                  : TabOwnership::CreatedForThisLoad);

    SettingsHandler::update_recent_files(file);
    mainwindow_.update_menu_and_actions();
}

int FileActions::save_file(CanvasView* canvasView, const QString& path)
{
    const QFile file(path);
    if (!file.exists()) {
        return save_file_as();
    }
    return write_to_path(canvasView, path);
}

int FileActions::write_to_path(CanvasView* canvasView, const QString& path)
{
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
        // Standard Save As semantics, and nothing more: THIS tab starts
        // living under the new path, the file it came from stays on disk
        // untouched and is not reopened.
        //
        // There used to be a branch here that, for an already-saved
        // document being saved under a different name, opened a SECOND
        // tab on the chosen path and started a background load of the
        // OLD file into it. Since TabPane::add_new_tab() makes the new
        // tab current, every current_widget() below then referred to
        // that new tab - so write_to_path() wrote out a scene the
        // background load had not populated yet (it is delivered through
        // ThreadedIO::finished, i.e. the event loop, which does not run
        // inside this call; ProgressDialog show()s rather than exec()s).
        // The chosen file got an EMPTY project written to it, with no
        // error, while the user's actual changes stayed behind in the
        // original tab, still unsaved.
        mainwindow_.tab_pane().set_current_tab_path(selected);
        mainwindow_.tab_pane().set_current_tab_title(
            QFileInfo(selected).fileName());
        mainwindow_.tab_pane().set_current_tab_project_name(
            QFileInfo(selected).fileName());

        // write_to_path(), not save_file(): the latter bails back into
        // THIS function when the path doesn't exist yet, which for a
        // genuinely new file meant mutual recursion. It was held off by
        // pre-touching the file into existence here first - with the
        // result of that open() discarded, so a directory that refused
        // the create (read-only, no permission, full disk) looped the
        // Save As dialog instead of reporting anything. FmlArchive::save()
        // writes through QSaveFile and creates the target itself, so
        // nothing has to exist beforehand.
        write_to_path(mainwindow_.tab_pane().current_widget(), selected);
        retval = QDialog::Accepted;
    }

    return retval;
}

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

void FileActions::newFile()
{
    mainwindow_.tabPane().addNewUntitledTab();
}

void FileActions::openFile()
{
    // SVG/Adobe filter entries used to be listed here too (pre-existing,
    // not something this refactor added) but loadFmlIntoCurrentTab()
    // only ever knows how to parse the .fml zip+manifest archive - a
    // raw .svg/.psd file just fails to open as one, so those filters
    // were pure misdirection (Max: "зачем там svg и psd?").
    const QStringList files = showOpenFilesDialog(&mainwindow_,
                                                  QObject::tr("Open"),
                                                  QDir::homePath(),
                                                  QStringLiteral("Familiar (*.fml)"));
    for (const QString& file : files)
        processOpenFile(file);
}

void FileActions::loadFmlIntoCurrentTab(const QString& path,
                                        bool markModifiedAfterLoad,
                                        const QUuid& recoveryIdToClear)
{
    CanvasView* canvasView = mainwindow_.tabPane().currentWidget();
    CanvasScene* scene = canvasView->scene();

    auto* worker = new ThreadedIO(
        [path, scene](ThreadedIO* w) { load_fml(path, scene, w); });

    QObject::connect(
        worker,
        &ThreadedIO::finished,
        &mainwindow_,
        [this, canvasView, scene, markModifiedAfterLoad, recoveryIdToClear](
            const QString& error, const QStringList& itemErrors) {
            scene->add_queued_items();
            // add_queued_items() populates the scene directly, bypassing
            // the undo stack entirely - the Hierarchy panel's rebuild
            // trigger is QUndoStack::indexChanged (see
            // resyncActionsForTab()), which won't fire for this, so it
            // needs an explicit nudge here.
            mainwindow_.notifyStructuralChange();
            // Before fit_scene(): seeds canvasRect_ from what
            // FmlArchive::load() stashed on the scene, so fit_scene()
            // has something to fall back to even if this project was
            // saved with zero items (see CanvasScene::
            // rememberedBoundingRect()).
            canvasView->restoreCanvasRect(scene->rememberedBoundingRect());
            canvasView->on_action_fit_scene();
            canvasView->setModified(markModifiedAfterLoad);

            // Only now - after the background read has actually
            // succeeded - is it safe to delete the recovery file it was
            // just read from. Also only on success: if loading failed,
            // keep the recovery snapshot around rather than silently
            // losing the only copy of that data.
            if (error.isEmpty() && !recoveryIdToClear.isNull())
                familiar::recovery::remove(recoveryIdToClear);

            if (!error.isEmpty()) {
                showMessageBox(QMessageBox::Critical,
                               &mainwindow_,
                               QObject::tr("Could not open file"),
                               error);
            }
            if (!itemErrors.isEmpty()) {
                QStringList lines;
                for (const QString& e : itemErrors) {
                    lines.append(QStringLiteral("<li>%1</li>").arg(e));
                }
                showMessageBox(QMessageBox::Warning,
                               &mainwindow_,
                               QObject::tr("Problem loading project"),
                               QObject::tr(
                                   "%1 item(s) could not be loaded.<ul>%2</ul>")
                                   .arg(itemErrors.size())
                                   .arg(lines.join(QString())));
            }
        });

    QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    new ProgressDialog(QObject::tr("Opening project"), worker, 0, &mainwindow_);
    worker->start();
}

void FileActions::restoreFromRecovery(const QString& recoveryFmlPath,
                                      const QString& originalPath,
                                      const QUuid& recoveryId)
{
    if (originalPath.isEmpty())
        mainwindow_.tabPane().addNewUntitledTab();
    else
        mainwindow_.tabPane().addNewTab(originalPath);
    loadFmlIntoCurrentTab(recoveryFmlPath,
                          /*markModifiedAfterLoad=*/true,
                          recoveryId);
}

CanvasView* FileActions::findBlankTab()
{
    TabPane& tp = mainwindow_.tabPane();
    for (int i = 0; i < tp.count(); ++i) {
        CanvasView* cv = tp.widgetAt(i);
        if (cv->isUntitled() && !cv->isModified())
            return cv;
    }
    return nullptr;
}

void FileActions::closeTab(CanvasView* cv)
{
    // Same direct `delete` TabPane::onTabClosed() uses for its own
    // "nothing to lose" branch - Qt's QTabWidget notices the child
    // widget being destroyed and removes its tab entry on its own, no
    // separate closeTabByIndex() call needed.
    delete cv;
}

void FileActions::processOpenFile(const QString& file)
{
    // Already open in some tab - switch to it instead of opening a
    // second copy. Was previously only checked in openFile()'s dialog
    // loop, so on_action_open_recent_file() (which calls this directly)
    // skipped it entirely.
    int count = mainwindow_.tabPane().count();
    for (int j = 0; j < count; ++j) {
        if (mainwindow_.tabPane().widgetAt(j)->path() == file) {
            mainwindow_.tabPane().setCurrentIndex(j);
            return;
        }
    }

    if (mainwindow_.tabPane().currentWidget()->isUntitled()
        && mainwindow_.tabPane().currentWidget()->isModified() == false) {
        mainwindow_.tabPane().setCurrentTabPath(file);
        mainwindow_.tabPane().setCurrentTabTitle(QFileInfo(file).fileName());
        mainwindow_.tabPane().setCurrentTabProjectName(
            QFileInfo(file).fileName());
    } else {
        mainwindow_.tabPane().addNewTab(file);
    }

    loadFmlIntoCurrentTab(file);

    SettingsHandler::getInstance()->updateRecentFiles(file);
    mainwindow_.update_menu_and_actions();
}

int FileActions::saveFile(CanvasView* canvasView, const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        return saveFileAs();
    }

    // Synchronous (not backgrounded like loadFmlIntoCurrentTab()): several
    // callers (TabPane::onTabClosed(), MainWindow::saveAllWindowSaveCB())
    // close the tab or quit the app right after this returns, assuming the
    // save has already completed - threading it would need those flows
    // reworked to wait on ThreadedIO::finished first.
    FmlResult result = FmlArchive::save(canvasView->scene(),
                                        canvasView->canvasRect(),
                                        path);

    if (!result.error.isEmpty()) {
        showMessageBox(QMessageBox::Critical,
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
        showMessageBox(QMessageBox::Warning,
                       &mainwindow_,
                       QObject::tr("Problem saving project"),
                       QObject::tr("%1 item(s) could not be saved.<ul>%2</ul>")
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
    canvasView->undoStack()->setClean();
    canvasView->setModified(false);

    // See processOpenFile()'s comment - saveFileAs() reaches this via
    // its own call to saveFile(selected), so this covers both.
    SettingsHandler::getInstance()->updateRecentFiles(path);
    mainwindow_.update_menu_and_actions();

    return QDialog::Accepted;
}

int FileActions::saveFile(const QString& path)
{
    return saveFile(mainwindow_.tabPane().currentWidget(), path);
}

int FileActions::saveFile()
{
    return saveFile(mainwindow_.tabPane().currentWidget()->path());
}

int FileActions::saveFileAs()
{
    int retval = QDialog::Rejected;
    // showSaveFileDialog() already appends the right extension for
    // whichever filter is selected (same job fileExt_ used to do here
    // manually) and confirms overwrite internally. SVG/Adobe filter
    // entries removed - saveFile()/FmlArchive only ever write the .fml
    // zip+manifest format, there's no actual SVG/PSD export path here
    // (Max: "зачем там svg и psd?").
    const QString selected = showSaveFileDialog(&mainwindow_,
                                                QObject::tr("Save As"),
                                                QDir::homePath(),
                                                QStringLiteral("Familiar (*.fml)"));

    if (!selected.isEmpty()) {
        if (!mainwindow_.tabPane().currentWidget()->isUntitled()
            && mainwindow_.tabPane().getCurrentTabProjectName()
                   != QFileInfo(selected).fileName()) {
            auto canvasView = mainwindow_.tabPane().currentWidget();
            mainwindow_.tabPane().addNewTab(selected);
            loadFmlIntoCurrentTab(canvasView->path());
        }

        mainwindow_.tabPane().setCurrentTabPath(selected);
        mainwindow_.tabPane().setCurrentTabTitle(QFileInfo(selected).fileName());
        mainwindow_.tabPane().setCurrentTabProjectName(
            QFileInfo(selected).fileName());

        // Pre-touch the file into existence: saveFile(path) bails into
        // saveFileAs() again if the path doesn't exist yet, which would
        // recurse right back here for a genuinely new file.
        (void) QFile(selected).open(QFile::ReadWrite);
        saveFile(selected);
        retval = QDialog::Accepted;
    }

    return retval;
}

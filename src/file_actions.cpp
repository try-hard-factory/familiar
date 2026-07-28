#include "file_actions.h"
#include "canvasscene.h"
#include "fileio.h"
#include "fml_archive.h"
#include "mainwindow.h"
#include "tabpane.h"
#include "widgets/dialogs.h"
#include <canvasview.h>
#include <core/settings.h>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QMessageBox>
#include <QString>

namespace {

// Shows the familiar mark for .fml files in the (non-native) open/save
// dialogs below, since the platform has no icon association for the
// extension registered (that's OS packaging, kept out of scope here -
// see docs/fml_format_design.en.md).
class FmlIconProvider : public QFileIconProvider
{
public:
    QIcon icon(const QFileInfo& info) const override
    {
        if (info.suffix().compare(QStringLiteral("fml"), Qt::CaseInsensitive)
            == 0) {
            static const QIcon fmlIcon(
                QStringLiteral(":/img/app/familiar_256.png"));
            return fmlIcon;
        }
        return QFileIconProvider::icon(info);
    }
};

} // namespace

FileActions::FileActions(MainWindow& mw)
    : mainwindow_(mw)
{
    fileExt_["Familiar (*.fml)"] = ".fml";
    fileExt_["SVG (*.svg)"] = ".svg";
    fileExt_["Adobe (*.psd)"] = ".psd";
}

FileActions::~FileActions() {}

void FileActions::newFile()
{
    mainwindow_.tabPane().addNewUntitledTab();
}

void FileActions::openFile()
{
    QFileDialog* fileDialog = new QFileDialog(&mainwindow_);
    // MainWindow is a translucent/frameless overlay (transparent
    // stylesheet + WA_TranslucentBackground); as a separate top-level
    // window without its own alpha channel, this dialog would otherwise
    // inherit "background: transparent" and paint solid black instead.
    fileDialog->setAttribute(Qt::WA_TranslucentBackground, false);
    // Clearing our own stylesheet isn't enough - Qt's QSS cascades from
    // MainWindow's "background: transparent" rule regardless, since an
    // empty child stylesheet doesn't cancel an inherited one. Define an
    // explicit competing rule here instead.
    fileDialog->setStyleSheet("* { background-color: palette(window); color: "
                              "palette(window-text); }");
    fileDialog->setNameFilter("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)");
    fileDialog->setDirectory(QDir::homePath());
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);
    // Static: setIconProvider() doesn't take ownership, must outlive fileDialog.
    static FmlIconProvider iconProvider;
    fileDialog->setIconProvider(&iconProvider);

    if (fileDialog->exec()) {
        if (fileDialog->selectedFiles().size() == 0) {
            return;
        }

        for (const QString& file : fileDialog->selectedFiles()) {
            processOpenFile(file);
        }
    }

    delete fileDialog;
}

void FileActions::loadFmlIntoCurrentTab(const QString& path)
{
    CanvasView* canvasView = mainwindow_.tabPane().currentWidget();
    CanvasScene* scene = canvasView->scene();

    auto* worker = new ThreadedIO(
        [path, scene](ThreadedIO* w) { load_fml(path, scene, w); });

    QObject::connect(
        worker,
        &ThreadedIO::finished,
        &mainwindow_,
        [this, canvasView, scene](const QString& error,
                                  const QStringList& itemErrors) {
            scene->add_queued_items();
            // Before fit_scene(): seeds canvasRect_ from what
            // FmlArchive::load() stashed on the scene, so fit_scene()
            // has something to fall back to even if this project was
            // saved with zero items (see CanvasScene::
            // rememberedBoundingRect()).
            canvasView->restoreCanvasRect(scene->rememberedBoundingRect());
            canvasView->on_action_fit_scene();
            canvasView->setModified(false);

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

    FamSettings().updateRecentFiles(file);
    mainwindow_.update_menu_and_actions();
}

int FileActions::saveFile(const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        return saveFileAs();
    }

    CanvasView* canvasView = mainwindow_.tabPane().currentWidget();

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
    FamSettings().updateRecentFiles(path);
    mainwindow_.update_menu_and_actions();

    return QDialog::Accepted;
}

int FileActions::saveFile()
{
    return saveFile(mainwindow_.tabPane().currentWidget()->path());
}

int FileActions::saveFileAs()
{
    int retval = QDialog::Rejected;
    QFileDialog* fileDialog = new QFileDialog(&mainwindow_);
    fileDialog->setAttribute(Qt::WA_TranslucentBackground, false);
    // Clearing our own stylesheet isn't enough - Qt's QSS cascades from
    // MainWindow's "background: transparent" rule regardless, since an
    // empty child stylesheet doesn't cancel an inherited one. Define an
    // explicit competing rule here instead.
    fileDialog->setStyleSheet("* { background-color: palette(window); color: "
                              "palette(window-text); }");
    fileDialog->setNameFilter("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)");
    fileDialog->setDirectory(QDir::homePath());
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
    fileDialog->setFileMode(QFileDialog::AnyFile);
    // Static: setIconProvider() doesn't take ownership, must outlive fileDialog.
    static FmlIconProvider iconProvider;
    fileDialog->setIconProvider(&iconProvider);
    auto retdialog = fileDialog->exec();

    if (retdialog == QDialog::Accepted) {
        QString selected = fileDialog->selectedFiles().at(0);
        if (!selected.contains(fileExt_.at(fileDialog->selectedNameFilter()),
                               Qt::CaseInsensitive)) {
            selected.append(fileExt_.at(fileDialog->selectedNameFilter()));
        }

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

        QFile(selected).open(QFile::ReadWrite);
        saveFile(selected);
        retval = QDialog::Accepted;
    }

    delete fileDialog;
    return retval;
}

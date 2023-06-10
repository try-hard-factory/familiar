#include "file_actions.h"
#include "fml_file_buffer.h"
#include "mainwindow.h"
#include "tabpane.h"
#include <canvasview.h>
#include <QFileDialog>
#include <QString>

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
    fileDialog->setNameFilter("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)");
    fileDialog->setDirectory(QDir::homePath());
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);

    if (fileDialog->exec()) {
        if (fileDialog->selectedFiles().size() == 0) {
            return;
        }

        QStringList selected = fileDialog->selectedFiles();
        for (int i = 0; i < selected.size(); ++i) {
            int count = mainwindow_.tabPane().count();
            bool found = false;
            for (int j = 0; j < count; ++j) {
                QString currentPath = mainwindow_.tabPane().widgetAt(j)->path();
                if (currentPath == selected.at(i)) {
                    mainwindow_.tabPane().setCurrentIndex(j);
                    found = true;
                    break;
                }
            }

            if (!found) {
                processOpenFile(selected.at(i));
            }
        }
    }

    delete fileDialog;
}

void FileActions::processOpenFile(const QString& file)
{
    if (mainwindow_.tabPane().currentWidget()->isUntitled()
        && mainwindow_.tabPane().currentWidget()->isModified() == false) {
        mainwindow_.tabPane().setCurrentTabPath(file);
        mainwindow_.tabPane().setCurrentTabTitle(QFileInfo(file).fileName());
        mainwindow_.tabPane().setCurrentTabProjectName(
            QFileInfo(file).fileName());
    } else {
        mainwindow_.tabPane().addNewTab(file);
    }

    fml_file_buffer::open_file(mainwindow_.tabPane().currentWidget(), file);
    mainwindow_.tabPane().currentWidget()->setModified(false);
}

int FileActions::saveFile(const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        return saveFileAs();
    }

    auto canvasWidget = mainwindow_.tabPane().currentWidget();
    QByteArray payload = fml_file_buffer::create_payload(canvasWidget);
    fml_file_buffer::save_to_file(path, payload);

    canvasWidget->setModified(false);

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
    fileDialog->setNameFilter("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)");
    fileDialog->setDirectory(QDir::homePath());
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
    fileDialog->setFileMode(QFileDialog::AnyFile);
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
            fml_file_buffer::open_file(mainwindow_.tabPane().currentWidget(),
                                       canvasView->path());
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

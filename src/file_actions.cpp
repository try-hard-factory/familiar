#include <QString>
#include <QFileDialog>
#include <canvasview.h>
#include "file_actions.h"
#include "tabpane.h"
#include "fml_file_buffer.h"
#include "mainwindow.h"

FileActions::FileActions(MainWindow* mw) : window_(mw)
{
    fileExt_["Familiar (*.fml)"] = ".fml";
    fileExt_["SVG (*.svg)"] = ".svg";
    fileExt_["Adobe (*.psd)"] = ".psd";
}

FileActions::~FileActions()
{

}

void FileActions::newFile()
{
    window_->tabPane()->addNewUntitledTab();
}

void FileActions::openFile()
{
    QFileDialog* fileDialog = new QFileDialog(window_);
    fileDialog->setNameFilter("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)");
    fileDialog->setDirectory( QDir::homePath() );
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);

    if (fileDialog->exec()) {
        if (fileDialog->selectedFiles().size() == 0) {
            return;
        }

        QStringList selected = fileDialog->selectedFiles();
        for (int i = 0; i < selected.size(); ++i) {
            int count = window_->tabPane()->count();
            bool found = false;
            for (int j = 0; j < count; ++j) {
                QString currentPath = window_->tabPane()->widgetAt(j)->path();
                if (currentPath == selected.at(i)) {
                    window_->tabPane()->setCurrentIndex(j);
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

void FileActions::processOpenFile(QString file)
{
    // recent logic
//    auto canvasView = tabpane_.currentWidget();
//    qDebug()<< canvasView << " " << canvasView->path()<<" "
//            << canvasView->isUntitled()<<" "
//            << canvasView->isModified();
    if ( window_->tabPane()->currentWidget()->isUntitled() &&  window_->tabPane()->currentWidget()->isModified() == false) {
        window_->tabPane()->setCurrentTabPath(file);
        window_->tabPane()->setCurrentTabTitle(QFileInfo(file).fileName());
    } else {
        window_->tabPane()->addNewTab(file);
    }

    fml_file_buffer::open_file(window_->tabPane()->currentWidget(), file);
    window_->tabPane()->currentWidget()->setModified(false);
}

int FileActions::saveFile(QString path)
{
    QFile file(path);
    if (!file.exists()) {
        return saveFileAs();
    }

    auto canvasWidget = window_->tabPane()->currentWidget();
    QString header_= QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
    QByteArray byteHeader = header_.toUtf8();
    QByteArray payload = fml_file_buffer::create_payload(canvasWidget);
    fml_file_buffer::save_to_file(path, byteHeader, payload);

    canvasWidget->setModified(false);

    return QDialog::Accepted;
}

int FileActions::saveFile()
{
    return saveFile(window_->tabPane()->currentWidget()->path());
}

int FileActions::saveFileAs()
{
    int retval = QDialog::Rejected;
    QFileDialog* fileDialog = new QFileDialog(window_);
    fileDialog->setNameFilter("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)");
    fileDialog->setDirectory( QDir::homePath() );
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
    fileDialog->setFileMode(QFileDialog::AnyFile);
    auto retdialog = fileDialog->exec();

    if (retdialog == QDialog::Accepted) {
        QString selected = fileDialog->selectedFiles().at(0);
        if (!selected.contains(fileExt_.at(fileDialog->selectedNameFilter()), Qt::CaseInsensitive)) {
            selected.append(fileExt_.at(fileDialog->selectedNameFilter()));
        }

        if (!window_->tabPane()->currentWidget()->isUntitled()) {
            auto canvasView = window_->tabPane()->currentWidget();
            window_->tabPane()->addNewTab(selected);
            fml_file_buffer::open_file(window_->tabPane()->currentWidget(), canvasView->path());
        }

        window_->tabPane()->setCurrentTabPath(selected);
        window_->tabPane()->setCurrentTabTitle(QFileInfo(selected).fileName());

        QFile(selected).open(QFile::ReadWrite);
        saveFile(selected);
        retval = QDialog::Accepted;
    }

    delete fileDialog;
    return retval;
}

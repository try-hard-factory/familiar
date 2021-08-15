#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QLayout>

#include "fml_file_buffer.h"
#include "project_settings.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent,Qt::Window
                  | Qt::CustomizeWindowHint
                  | Qt::WindowSystemMenuHint
                  | Qt::WindowMinimizeButtonHint
                  | Qt::WindowMaximizeButtonHint
                  | Qt::WindowCloseButtonHint)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    projectSettings_ = new project_settings(this);
    canvasWidget = new CanvasView();
    fileDialog_ = new QFileDialog(this);

    canvasWidget->setProjectSettings(projectSettings_);

    fileDialog_->setNameFilter(tr("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)"));
    fileExt_["Familiar (*.fml)"] = ".fml";
    fileExt_["SVG (*.svg)"] = ".svg";
    fileExt_["Adobe (*.psd)"] = ".psd";

    fileDialog_->setDirectory( QDir::homePath() );
    fileDialog_->setOption(QFileDialog::DontUseNativeDialog, true);


    qreal x = 000;
//    canvasWidget->addImage("kot.png", {-1000, -1000-x});

//    canvasWidget->addImage("kot2.jpg", {50,100-x});
//    canvasWidget->addImage("kot2.jpg", {50,400-x});
//    canvasWidget->addImage("kot2.jpg", {250,50-x});
//    canvasWidget->addImage("kot2.jpg", {450,50-x});

//    canvasWidget->addImage("kot1.png", {50,100-x});
//    canvasWidget->addImage("kot1.png", {50,400-x});
//    canvasWidget->addImage("kot1.png", {250,50-x});
//    canvasWidget->addImage("kot1.png", {450,50-x});

    canvasWidget->addImage("kot1.png", {200-x, 150-x});
    canvasWidget->addImage("kot1.png", {450-x, 50-x});
    canvasWidget->addImage("kot.png", {700-x, 0-x});

//    canvasWidget->addImage("bender.png", {1300,1300});

//    for (int i=0;i<100;++i)
//        canvasWidget->addImage(qimage,{(double)i*100,(double)i*100});

//    canvasWidget->setBackgroundBrush(QBrush(QColor(0xFF,0xFF,0xFF)));
    setCentralWidget(canvasWidget);
    canvasWidget->show();
}


MainWindow::~MainWindow()
{
    delete fileDialog_;
    delete ui;
    delete canvasWidget;
}


void MainWindow::on_action_saveas_triggered()
{
    fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

    if (fileDialog_->exec() == QDialog::Accepted) {
        QString out_file(fileDialog_->selectedFiles().value(0));
        if (!out_file.contains(fileExt_.at(fileDialog_->selectedNameFilter()), Qt::CaseInsensitive)) {
            out_file.append(fileExt_.at(fileDialog_->selectedNameFilter()));
        }

        QFileInfo fi(out_file);
        projectSettings_->title(fi.fileName());
        projectSettings_->path(fi.absolutePath());

        qDebug()<<out_file;
        qDebug()<<fileDialog_->directory();
        QString header_=QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
        QByteArray byteHeader = header_.toUtf8();
        qDebug()<<"Header "<<header_;
        QByteArray payload = fml_file_buffer::create_payload(canvasWidget);

        fml_file_buffer::save_to_file(out_file, byteHeader, payload);
    }
}


void MainWindow::on_action_open_triggered()
{
    fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    if (fileDialog_->exec() == QDialog::Accepted) {
        qDebug()<< fileDialog_->selectedFiles().value(0);
        QString file_name(fileDialog_->selectedFiles().value(0));
        QFileInfo fi(file_name);
        projectSettings_->title(fi.fileName());
        projectSettings_->path(fi.absolutePath());
        projectSettings_->change(false);

        fml_file_buffer::open_file(canvasWidget, file_name);
    }
}

void MainWindow::on_action_quit_triggered()
{
    qDebug()<<"on_action_quit_triggered "<<projectSettings_->change();
    if (projectSettings_->change() == false) {
        QApplication::quit();
    } else {
        fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

        if (fileDialog_->exec() == QDialog::Accepted) {
            QString out_file(fileDialog_->selectedFiles().value(0));
            if (!out_file.contains(fileExt_.at(fileDialog_->selectedNameFilter()), Qt::CaseInsensitive)) {
                out_file.append(fileExt_.at(fileDialog_->selectedNameFilter()));
            }

            QFileInfo fi(out_file);
            projectSettings_->title(fi.fileName());
            projectSettings_->path(fi.absolutePath());

            qDebug()<<out_file;
            qDebug()<<fileDialog_->directory();
            QString header_=QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
            QByteArray byteHeader = header_.toUtf8();
            qDebug()<<"Header "<<header_;
            QByteArray payload = fml_file_buffer::create_payload(canvasWidget);

            fml_file_buffer::save_to_file(out_file, byteHeader, payload);
            QApplication::quit();
        } else {//cancel

        }
    }
}

void MainWindow::on_action_save_triggered()
{
    if (projectSettings_->isDefaultProjectName()) {
        fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

        if (fileDialog_->exec() == QDialog::Accepted) {
            QString out_file(fileDialog_->selectedFiles().value(0));
            if (!out_file.contains(fileExt_.at(fileDialog_->selectedNameFilter()), Qt::CaseInsensitive)) {
                out_file.append(fileExt_.at(fileDialog_->selectedNameFilter()));
            }

            QFileInfo fi(out_file);
            projectSettings_->title(fi.fileName());
            projectSettings_->path(fi.absolutePath());

            qDebug()<<out_file;
            qDebug()<<fileDialog_->directory();
            QString header_=QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
            QByteArray byteHeader = header_.toUtf8();
            qDebug()<<"Header "<<header_;
            QByteArray payload = fml_file_buffer::create_payload(canvasWidget);

            fml_file_buffer::save_to_file(out_file, byteHeader, payload);

            projectSettings_->change(false);
        } else {//cancel

        }
    } else {
        QString out_file(projectSettings_->path() + projectSettings_->title());
        projectSettings_->title(projectSettings_->title());

        QString header_=QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
        QByteArray byteHeader = header_.toUtf8();
        qDebug()<<"Header "<<header_;
        QByteArray payload = fml_file_buffer::create_payload(canvasWidget);

        fml_file_buffer::save_to_file(out_file, byteHeader, payload);

        projectSettings_->change(false);
    }
}

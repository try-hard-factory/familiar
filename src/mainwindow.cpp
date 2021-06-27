#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QLayout>

#include "fml_file_buffer.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    canvasWidget = new CanvasView();
    fileDialog_ = new QFileDialog(this);
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

//    canvasWidget->addImage("kot1.png", {200-x, 50-x});
//    canvasWidget->addImage("kot1.png", {450-x, 50-x});
//    canvasWidget->addImage("kot1.png", {1000-x, 250-x});
//    canvasWidget->addImage("kot1.png", {1000-x, 600-x});
//    canvasWidget->addImage("kot1.png", {600-x, 600-x});
//    canvasWidget->addImage("kot1.png", {200-x, 600-x});
//    canvasWidget->addImage("kot1.png", {800-x, 400-x});
//    canvasWidget->addImage("kot.png", {200-x, 200-x});
//    canvasWidget->addImage("kot.png", {700-x, 50-x});

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
        QString out_file(fileDialog_->selectedFiles().value(0) + fileExt_.at(fileDialog_->selectedNameFilter()));
        qDebug()<<out_file;
        qDebug()<<fileDialog_->directory();
        auto header = fml_file_buffer::create_header(canvasWidget);
        QString header_=QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
        QByteArray byteHeader = header_.toUtf8();
        qDebug()<<"Header "<<header_;
        QByteArray payload = fml_file_buffer::create_payload(canvasWidget);

        fml_file_buffer::save_to_file(out_file, byteHeader, payload);

        QImage img((const unsigned char*)payload.data(), 200, 200, QImage::Format_ARGB32);
        canvasWidget->addImage(img, {0, 0});
    }
}


void MainWindow::on_action_open_triggered()
{
    fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    if (fileDialog_->exec() == QDialog::Accepted) {
        qDebug()<< fileDialog_->selectedFiles().value(0);
        fml_file_buffer::open_file(canvasWidget, fileDialog_->selectedFiles().value(0));
    }
}

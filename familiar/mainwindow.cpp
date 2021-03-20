#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    canvasWidget = new CanvasView();

    auto qimage = QImage("kot.jpg");
    qInfo() << qimage.width() << ' ' <<qimage.height();

    canvasWidget->addImage(qimage, {0,0});
    canvasWidget->addImage(qimage, {100,100});
    canvasWidget->addImage(qimage, {200,200});
    canvasWidget->addImage(qimage, {300,300});

//    for (int i=0;i<100;++i)
//        canvasWidget->addImage(qimage,{(double)i*100,(double)i*100});

    canvasWidget->setBackgroundBrush(QBrush(QColor(0xFF,0xFF,0xFF)));
    setCentralWidget(canvasWidget);

}

MainWindow::~MainWindow()
{
    delete ui;
    delete canvasWidget;
}


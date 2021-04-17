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

    canvasWidget->addImage("kot.png", {700,50});
    canvasWidget->addImage("kot.png", {300,200});
    canvasWidget->addImage("kot1.png", {200,600});
    canvasWidget->addImage("kot1.png", {400,50});
    canvasWidget->addImage("kot1.png", {200,50});
    canvasWidget->addImage("kot1.png", {600,600});
    canvasWidget->addImage("kot1.png", {1000,600});
    canvasWidget->addImage("kot1.png", {1000,300});
    canvasWidget->addImage("kot1.png", {600,400});
//    canvasWidget->addImage("bender.png", {300,300});

//    for (int i=0;i<100;++i)
//        canvasWidget->addImage(qimage,{(double)i*100,(double)i*100});

//    canvasWidget->setBackgroundBrush(QBrush(QColor(0xFF,0xFF,0xFF)));
    setCentralWidget(canvasWidget);

}

MainWindow::~MainWindow()
{
    delete ui;
    delete canvasWidget;
}

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

    qreal x = 000;
    canvasWidget->addImage("kot.png", {700,50-x});
    canvasWidget->addImage("kot.png", {300,200-x});
    canvasWidget->addImage("kot1.png", {200,600-x});
    canvasWidget->addImage("kot1.png", {400,50-x});
    canvasWidget->addImage("kot1.png", {00,00-x});
    canvasWidget->addImage("kot1.png", {200,50-x});
    canvasWidget->addImage("kot1.png", {200,100-x});
    canvasWidget->addImage("kot1.png", {200,150-x});
    canvasWidget->addImage("kot1.png", {600,400-x});
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

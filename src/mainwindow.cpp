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
    canvasWidget->addImage("kot.png", {-1000, -1000-x});

//    canvasWidget->addImage("kot1.png", {50,100-x});
//    canvasWidget->addImage("kot1.png", {50,400-x});
//    canvasWidget->addImage("kot1.png", {250,50-x});
//    canvasWidget->addImage("kot1.png", {450,50-x});

    canvasWidget->addImage("kot1.png", {200-x, 50-x});
    canvasWidget->addImage("kot1.png", {450-x, 50-x});
    canvasWidget->addImage("kot1.png", {1000-x, 250-x});
    canvasWidget->addImage("kot1.png", {1000-x, 600-x});
    canvasWidget->addImage("kot1.png", {600-x, 600-x});
    canvasWidget->addImage("kot1.png", {200-x, 600-x});
    canvasWidget->addImage("kot1.png", {800-x, 400-x});
    canvasWidget->addImage("kot.png", {200-x, 200-x});
    canvasWidget->addImage("kot.png", {700-x, 50-x});

    canvasWidget->addImage("bender.png", {1300,1300});

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


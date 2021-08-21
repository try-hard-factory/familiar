#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QLayout>
#include <QLabel>
#include <QPushButton>

#include "fml_file_buffer.h"
#include "project_settings.h"
#include "tabpane.h"
#include "saveallwindow.h"

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
    setWindowTitle("Familiar");
//    projectSettings_ = new project_settings(this);
//    canvasWidget = new CanvasView();
//    fileDialog_ = new QFileDialog(this);

//    canvasWidget->setProjectSettings(projectSettings_);

//    fileDialog_->setNameFilter(tr("Familiar (*.fml);; SVG (*.svg);; Adobe (*.psd)"));
    fileExt_["Familiar (*.fml)"] = ".fml";
    fileExt_["SVG (*.svg)"] = ".svg";
    fileExt_["Adobe (*.psd)"] = ".psd";

//    fileDialog_->setDirectory( QDir::homePath() );
//    fileDialog_->setOption(QFileDialog::DontUseNativeDialog, true);


//    qreal x = 000;
//    canvasWidget->addImage("kot.png", {-1000, -1000-x});

//    canvasWidget->addImage("kot2.jpg", {50,100-x});
//    canvasWidget->addImage("kot2.jpg", {50,400-x});
//    canvasWidget->addImage("kot2.jpg", {250,50-x});
//    canvasWidget->addImage("kot2.jpg", {450,50-x});

//    canvasWidget->addImage("kot1.png", {50,100-x});
//    canvasWidget->addImage("kot1.png", {50,400-x});
//    canvasWidget->addImage("kot1.png", {250,50-x});
//    canvasWidget->addImage("kot1.png", {450,50-x});

//    canvasWidget->addImage("kot1.png", {200-x, 150-x});
//    canvasWidget->addImage("kot1.png", {450-x, 50-x});
//    canvasWidget->addImage("kot.png", {700-x, 0-x});

//    canvasWidget->addImage("bender.png", {1300,1300});

//    for (int i=0;i<100;++i)
//        canvasWidget->addImage(qimage,{(double)i*100,(double)i*100});

//    canvasWidget->setBackgroundBrush(QBrush(QColor(0xFF,0xFF,0xFF)));

    fileactions_ = new FileActions(this);
    tabpane_ = new TabPane(*fileactions_);
    setCentralWidget(tabpane_);
}


MainWindow::~MainWindow()
{
    delete tabpane_;
    delete ui;
}

void MainWindow::newFile()
{
    fileactions_->newFile();
}

void MainWindow::openFile()
{
    fileactions_->openFile();
}


bool MainWindow::checkSave()
{
    bool found = false;
    QStringList items;
    int count = tabpane_->count();
    for (int i = 0; i<count; i++) {
        if (tabpane_->widgetAt(i)->isModified()) {
            found = true;
            items.push_back(tabpane_->widgetAt(i)->path());
        }
    }

    if (found) {
        QString details = "";
        for (int i = 0; i<items.size(); i++) {
            details+=items.at(i)+"\n";
        }

        QMessageBox msg(this);
        msg.setWindowTitle("Warning!");
        msg.setText("You have unsaved documents!\n"
                    "Do you wish to exit?");
        msg.setDetailedText(details);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setDefaultButton(QMessageBox::No);
        msg.setIcon(QMessageBox::Warning);

        int ret = msg.exec();

        if (ret==QMessageBox::Yes) {
            return true;
        }
        return false;
    }
    return true;
}

int MainWindow::saveFile()
{
    return fileactions_->saveFile();
}

int MainWindow::saveFileAs()
{
    return fileactions_->saveFileAs();
}

void MainWindow::quitProject()
{
    if (checkSave()) {
//        QApplication::quit();
        qApp->exit(0); // Is it correct way?
    }
}

void MainWindow::cleanupWorkplace()
{
    //    canvasWidget->cleanupWorkplace();
}

TabPane *MainWindow::tabPane()
{
    return tabpane_;
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (checkSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}


void MainWindow::on_action_saveas_triggered()
{
    saveFileAs();
}


void MainWindow::on_action_open_triggered()
{
    openFile();
}

void MainWindow::on_action_quit_triggered()
{
    quitProject();
}

void MainWindow::on_action_save_triggered()
{
    saveFile();
}

void MainWindow::on_action_settings_triggered()
{

//    QWidget *pLineEdit = new QWidget(this);
//    pLineEdit->setWindowModality(Qt::ApplicationModal);
//    pLineEdit->setWindowFlags(Qt::Tool | Qt::Dialog);
//    pLineEdit->show ();

//    QWidget* widget = new QWidget();
//    widget->setWindowModality(Qt::ApplicationModal);
//    widget->setWindowFlags(Qt::Tool | Qt::Dialog);
//    widget->resize(320, 240);
//    QVBoxLayout *l = new QVBoxLayout;
//    l->setContentsMargins(0,0,0,0);
//    widget->setLayout(l);
//    QLabel *lab = new QLabel;
//    lab->setText("Hui");
//    l->addWidget(lab);
//    QPushButton* pb = new QPushButton;
//    pb->setText("close without save");

//    l->addWidget(pb);
//    qDebug()<<"on_action_settings_triggered "<<widget->size();
//    {
//        auto host = this;

//        auto hostRect = host->geometry();
//        auto point = hostRect.center() - QPoint(widget->size().width()/2,
//                                                widget->size().height()/2);
//        widget->move(point);
//     }

    SaveAllWindow* widget = new SaveAllWindow(this);
    widget->show();
}

void MainWindow::on_action_new_triggered()
{
    newFile();
}

void MainWindow::on_action_save_all_triggered()
{

}

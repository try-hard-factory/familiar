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
#include <map>

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
    std::map<int, QString> items;
    int count = tabpane_->count();
    for (int i = 0; i<count; i++) {
        if (tabpane_->widgetAt(i)->isModified()) {
            found = true;
            items.emplace(i, tabpane_->widgetAt(i)->path());

        }
    }

    if (found) {
        QString details = "";
        for (auto&[_, p] : items) {
            details+=p+"\n";
        }
        SaveAllWindow* widget = new SaveAllWindow(this, items);
        widget->setAttribute( Qt::WA_DeleteOnClose );
        widget->show();
//        QMessageBox msg(this);
//        msg.setWindowTitle("Warning!");
//        msg.setText("You have unsaved documents!\n"
//                    "Do you wish to exit?");
//        msg.setDetailedText(details);
//        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
//        msg.setDefaultButton(QMessageBox::No);
//        msg.setIcon(QMessageBox::Warning);

//        int ret = msg.exec();

//        if (ret==QMessageBox::Yes) {
//            return true;
//        }
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
        exitProject();
    }
}

void MainWindow::saveAllWindowSaveCB(SaveAllWindow* w, std::map<int, bool> &&m)
{
    w->close();

    int exit_flag = 1;

    for (auto it = m.rbegin(); it != m.rend(); it++) {
        if (!it->second) {
            qDebug()<<"close ID = "<<it->first<<" "<<tabpane_->getCurrentTabPath();
            tabpane_->closeTabByIndex(it->first);
        }
    }

    for (int i = tabpane_->count()-1; i>=0; --i) {
        qDebug()<<"save ID = "<<i<<" "<<tabpane_->getCurrentTabPath();
        tabpane_->setCurrentIndex(i);

        auto ret = fileactions_->saveFile();
        if (ret == QDialog::Rejected) {
            exit_flag = 0;
        } else {
            tabpane_->closeTabByIndex(i);
        }

    }
    if (exit_flag) exitProject();
}

void MainWindow::cleanupWorkplace()
{
    //    canvasWidget->cleanupWorkplace();
}

void MainWindow::exitProject()
{
    qApp->exit(0);// Is it correct way?
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

}

void MainWindow::on_action_new_triggered()
{
    newFile();
}

void MainWindow::on_action_save_all_triggered()
{
    for (int i = tabpane_->count()-1; i>=0; --i) {
        tabpane_->setCurrentIndex(i);

        auto ret = fileactions_->saveFile();
        (void) ret;
    }
}

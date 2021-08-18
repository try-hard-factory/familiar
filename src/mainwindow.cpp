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

void MainWindow::openFile()
{
    int ret = -1;
    if (projectSettings_->modified() == true) {
        ret = messageShow("Warning!", "You have unsaved documents!\n"
                "Do you wish to continue?", QMessageBox::Yes | QMessageBox::No);
    }

    if (ret == QMessageBox::No) return;

    cleanupWorkplace();

    fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    if (fileDialog_->exec() == QDialog::Accepted) {
        qDebug()<< fileDialog_->selectedFiles().value(0);
        QString file_name(fileDialog_->selectedFiles().value(0));
        QFileInfo fi(file_name);
        projectSettings_->title(fi.fileName());
        projectSettings_->path(fi.absoluteFilePath());
        projectSettings_->modified(false);

        fml_file_buffer::open_file(canvasWidget, file_name);
    }
}

void MainWindow::saveFile(const QString& path)
{
    QString header_= QString::fromStdString(fml_file_buffer::create_header(canvasWidget));
    QByteArray byteHeader = header_.toUtf8();
    QByteArray payload = fml_file_buffer::create_payload(canvasWidget);
    fml_file_buffer::save_to_file(path, byteHeader, payload);
}

void MainWindow::saveFile()
{
    QFile file(projectSettings_->path());
    if (!file.exists()) {
        saveFileAs();
        return;
    }

    QString out_file(projectSettings_->path());
    projectSettings_->title(projectSettings_->title());

    saveFile(out_file);

    projectSettings_->modified(false);
}

void MainWindow::saveFileAs()
{
    fileDialog_->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

    if (fileDialog_->exec() == QDialog::Accepted) {
        QString out_file(fileDialog_->selectedFiles().value(0));
        if (!out_file.contains(fileExt_.at(fileDialog_->selectedNameFilter()), Qt::CaseInsensitive)) {
            out_file.append(fileExt_.at(fileDialog_->selectedNameFilter()));
        }

        QFileInfo fi(out_file);
        projectSettings_->title(fi.fileName());
        projectSettings_->path(fi.absoluteFilePath());

        saveFile(out_file);

        projectSettings_->modified(false);
    }
}

void MainWindow::quitProject()
{
    if (projectSettings_->modified() == false) {
        QApplication::quit();
    } else {
        int ret = messageShow("Warning!", "You have unsaved documents!", QMessageBox::Save | QMessageBox::Cancel);

        if (ret == QMessageBox::Save) {
            saveFile();
            QApplication::quit();
        } else if (ret == QMessageBox::Cancel) {
            return;
        }
    }
}

void MainWindow::cleanupWorkplace()
{
    canvasWidget->cleanupWorkplace();
}

void centerWidget(QWidget *widget, QWidget *host = Q_NULLPTR) {
    if (!host) {
        host = widget->parentWidget();
    }

    if (host) {
        auto hostRect = host->geometry();
        auto point = hostRect.center() - QPoint(widget->baseSize().width()/2,
                                                widget->baseSize().height()/2);
        widget->move(point);
    } else {
        QRect screenGeometry = QGuiApplication::screens()[0]->geometry();
        int x = (screenGeometry.width() - widget->width()) / 2;
        int y = (screenGeometry.height() - widget->height()) / 2;
        widget->move(x, y);
    }
}


int MainWindow::messageShow(const QString& title, const QString& text, QMessageBox::StandardButtons b)
{
    QMessageBox msg;
    msg.setBaseSize(QSize(300,140));// I don't think that it is correct way.
                                    // But I have to use it because QMessageBox::size()
                                    // method returns the wrong value that I expect = (640x480)
                                    // I think i'll create custom messages window
    msg.setWindowTitle(title);
    msg.setIcon(QMessageBox::Warning);
    msg.setText(text);

    msg.setStandardButtons(b);
    msg.setDefaultButton(QMessageBox::No);

    centerWidget(&msg, this);

    return msg.exec();
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
cleanupWorkplace();
}

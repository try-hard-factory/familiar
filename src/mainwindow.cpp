#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>

#include "fml_file_buffer.h"
#include "project_settings.h"
#include "tabpane.h"
#include "saveallwindow.h"
#include <ui/settings_window.h>
#include <core/settingshandler.h>
#include <map>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent,Qt::Window
                  | Qt::CustomizeWindowHint
                  | Qt::WindowSystemMenuHint
                  | Qt::WindowMinimizeButtonHint
                  | Qt::WindowMaximizeButtonHint
                  | Qt::WindowCloseButtonHint)
    , ui(new Ui::MainWindow),
      fileactions_(new FileActions(*this)),
      tabpane_(new TabPane(*this))
{
    ui->setupUi(this);
    setWindowTitle("Familiar");
    statusBar()->hide();
    //this->setWindowFlags(Qt::WindowTransparentForInput|Qt::WindowStaysOnTopHint);

    createActions();
    createMenus();

    initShortcuts();

    setCentralWidget(tabpane_);
}


MainWindow::~MainWindow()
{
    delete tabpane_;
    delete ui;
}


void MainWindow::quitProject()
{
    if (checkSave()) {
        exitProject();
    }
}


void MainWindow::saveAll()
{
    for (int i = tabpane_->count()-1; i>=0; --i) {
        tabpane_->setCurrentIndex(i);

        auto ret = fileactions_->saveFile();
        (void) ret;
    }
}


void MainWindow::newFile()
{
    fileactions_->newFile();
}


void MainWindow::settingsWindow()
{
    SettingsWindow* widget = new SettingsWindow(this);
    widget->setAttribute( Qt::WA_DeleteOnClose );
    widget->show();
    centered_widget(this, widget);
}


void MainWindow::saveFile()
{
    fileactions_->saveFile();
}


void MainWindow::quit()
{
    quitProject();
}

void MainWindow::openFile()
{
    fileactions_->openFile();
}


void MainWindow::saveFileAs()
{
    fileactions_->saveFileAs();
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

        return false;
    }
    return true;
}

void MainWindow::initShortcuts()
{
    newShortcut(QKeySequence(SettingsHandler().shortcut("TYPE_SAVE")), this, SLOT(saveFile()));
    newShortcut(QKeySequence(SettingsHandler().shortcut("TYPE_QUIT")), this, SLOT(quit()));
}

QList<QShortcut*> MainWindow::newShortcut(const QKeySequence& key, QWidget* parent, const char* slot)
{
    QList<QShortcut*> shortcuts;
    QString strKey = key.toString();
    if (strKey.contains("Enter") || strKey.contains("Return")) {
        strKey.replace("Enter", "Return");
        shortcuts << new QShortcut(strKey, parent, slot);
        strKey.replace("Return", "Enter");
        shortcuts << new QShortcut(strKey, parent, slot);
    } else {
        shortcuts << new QShortcut(key, parent, slot);
    }
    return shortcuts;
}

void MainWindow::createActions()
{
    saveAllAction_ = new QAction(tr("Save all"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    saveAllAction_->setStatusTip(tr("Save all"));
    connect(saveAllAction_, &QAction::triggered, this, &MainWindow::saveAll);

    newAction_ = new QAction(tr("New"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    newAction_->setStatusTip(tr("New"));
    connect(newAction_, &QAction::triggered, this, &MainWindow::newFile);

    settingsAction_ = new QAction(tr("Settings"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    settingsAction_->setStatusTip(tr("Settings"));
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::settingsWindow);

    saveAction_ = new QAction(tr("Save"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    saveAction_->setStatusTip(tr("Save"));
    connect(saveAction_, &QAction::triggered, this, &MainWindow::saveFile);

    quitAction_ = new QAction(tr("Quit"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    quitAction_->setStatusTip(tr("Quit"));
    connect(quitAction_, &QAction::triggered, this, &MainWindow::quit);

    openAction_ = new QAction(tr("Open"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    openAction_->setStatusTip(tr("Open"));
    connect(openAction_, &QAction::triggered, this, &MainWindow::openFile);

    saveAsAction_ = new QAction(tr("Save As"), this);
//    saveAllAction_->setShortcuts(QKeySequence::New);
    saveAsAction_->setStatusTip(tr("Save As"));
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::saveFileAs);
}

void MainWindow::createMenus()
{
    fileMenu_ = menuBar()->addMenu(tr("menu"));
    fileMenu_->addAction(newAction_);
    fileMenu_->addAction(openAction_);
    fileMenu_->addAction(saveAction_);
    fileMenu_->addAction(saveAsAction_);
    fileMenu_->addAction(saveAllAction_);
    fileMenu_->addAction(settingsAction_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(quitAction_);
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

TabPane& MainWindow::tabPane()
{
    return *tabpane_;
}

FileActions &MainWindow::fileActions()
{
    return *fileactions_;
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (checkSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}


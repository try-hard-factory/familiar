#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>

#include "fml_file_buffer.h"
#include "project_settings.h"
#include "saveallwindow.h"
#include "tabpane.h"
#include <core/settingshandler.h>
#include <map>
#include <ui/settings_window.h>

#include "Logger.h"

extern Logger logger;

static QHash<QString, EShortcutButtons> recognizedShortcutsActions = {
    {"TYPE_NEW", k_TYPE_NEW},
    {"TYPE_OPEN", k_TYPE_OPEN},
    {"TYPE_SAVE", k_TYPE_SAVE},
    {"TYPE_QUIT", k_TYPE_QUIT},
};


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent,
                  Qt::Window | Qt::CustomizeWindowHint | Qt::WindowSystemMenuHint
                      | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                      | Qt::WindowCloseButtonHint)
    , ui(new Ui::MainWindow)
    , fileactions_(new FileActions(*this))
    , tabpane_(new TabPane(this, *this))
{
    ui->setupUi(this);
    setWindowTitle("Familiar");
    statusBar()->hide();

    setMouseTracking(true);
    //this->setWindowFlags(Qt::WindowTransparentForInput|Qt::WindowStaysOnTopHint);

    initShortcuts();

    createActions();
    createMenus();

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::shortCutChanged,
            this,
            &MainWindow::notifyShortcut);

    auto colorPreset = settings_.getCurrentColorPreset();
    backGroundColor_ = colorPreset[EPresetsColorIdx::kBackgroundColor];
    currentOpacity_ = settings_.getCurrentOpacity();
    rgbaBackGroundStr_ = QString("rgba(%1, %2, %3, %4);")
                             .arg(backGroundColor_.red())
                             .arg(backGroundColor_.green())
                             .arg(backGroundColor_.blue())
                             .arg(currentOpacity_);

    tabpane_->setWindowFlags(Qt::FramelessWindowHint);
    tabpane_->setAttribute(Qt::WA_TranslucentBackground);
    tabpane_->setStyleSheet("QTabBar::tab { background: rgba(255, 255, 0, 128); } QTabWidget::pane { border: "
                            "1px solid lightgray; top:-1px; background:  transparent; }");

    //tabpane_->setStyleSheet("background: transparent; background-color: rgba(255, 255, 0, 128);");
    setCentralWidget(tabpane_);

    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(
        Qt::Window
        | Qt::FramelessWindowHint); //|Qt::WindowTransparentForInput|Qt::WindowStaysOnTopHint);

    setStyleSheet("background: transparent; background-color: transparent;");// + rgbaBackGroundStr_);
    // Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowStaysOnTopHint;
    // flags &= ~Qt::WindowTransparentForInput; // Опускаем последний бит
    // setWindowFlags(flags);
    // setWindowOpacity(0.6);
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
    for (int i = tabpane_->count() - 1; i >= 0; --i) {
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
    widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint
                           | Qt::MSWindowsFixedSizeDialogHint);
    widget->raise();
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

void MainWindow::notifyShortcut(const QString& actionName)
{
    auto settings = SettingsHandler::getInstance();
    LOG_WARNING(logger,
                "notifyShortcut. actionName: ",
                actionName.toStdString(),
                ", shortcut: ",
                settings->shortcut(actionName).toStdString());
    auto idx = recognizedShortcutsActions[actionName];
    auto keyseq = QKeySequence(SettingsHandler().shortcut(actionName));
    actionsArr_[idx]->setShortcut(keyseq);
}

void MainWindow::settingsChangedSlot()
{
    auto colorPreset = settings_.getCurrentColorPreset();
    backGroundColor_ = colorPreset[EPresetsColorIdx::kBackgroundColor];
    currentOpacity_ = settings_.getCurrentOpacity();
    rgbaBackGroundStr_ = QString("rgba(%1, %2, %3, %4);")
                             .arg(backGroundColor_.red())
                             .arg(backGroundColor_.green())
                             .arg(backGroundColor_.blue())
                             .arg(currentOpacity_);

    tabpane_->setStyleSheet("QTabBar::tab { background: rgba(255, 255, 0, 128); } QTabWidget::pane { border: "
                            "1px solid lightgray; top:-1px; background:  transparent; }");
    setStyleSheet("background: transparent; background-color: transparent; ");// + rgbaBackGroundStr_);
}


bool MainWindow::checkSave()
{
    bool found = false;
    std::map<int, QString> items;
    int count = tabpane_->count();
    for (int i = 0; i < count; i++) {
        if (tabpane_->widgetAt(i)->isModified()) {
            found = true;
            items.emplace(i, tabpane_->widgetAt(i)->path());
        }
    }

    if (found) {
        QString details = "";
        for (auto& [_, p] : items) {
            details += p + "\n";
        }
        SaveAllWindow* widget = new SaveAllWindow(this, items);
        widget->setAttribute(Qt::WA_DeleteOnClose);
        widget->show();

        return false;
    }
    return true;
}

void MainWindow::initShortcuts()
{
    //    newShortcut(k_TYPE_SAVE, QKeySequence(SettingsHandler().shortcut("TYPE_SAVE")), this, SLOT(saveFile()));
    //    newShortcut(k_TYPE_QUIT, QKeySequence(SettingsHandler().shortcut("TYPE_QUIT")), this, SLOT(quit()));
}

void MainWindow::newShortcut(EShortcutButtons as_key,
                             const QKeySequence& key,
                             QWidget* parent,
                             const char* slot)
{
    //    QString strKey = key.toString();
    //    QShortcut* sc = nullptr;
    //    if (strKey.contains("Enter") || strKey.contains("Return")) {
    //        strKey.replace("Enter", "Return");
    //        sc = new QShortcut(strKey, parent, slot);
    //        strKey.replace("Return", "Enter");
    //        sc = new QShortcut(strKey, parent, slot);
    //    } else {
    //        sc = new QShortcut(key, parent, slot);
    //    }

    //    shortcutArr_[as_key] = sc;
}

void MainWindow::createActions()
{
    auto settings = SettingsHandler::getInstance();
    saveAllAction_ = new QAction(tr("Save all"), this);
    //    saveAllAction_->setShortcuts(QKeySequence::New);
    saveAllAction_->setStatusTip(tr("Save all"));
    connect(saveAllAction_, &QAction::triggered, this, &MainWindow::saveAll);

    newAction_ = new QAction(tr("New"), this);
    newAction_->setShortcut(QKeySequence(settings->shortcut("TYPE_NEW")));
    newAction_->setStatusTip(tr("New"));
    connect(newAction_, &QAction::triggered, this, &MainWindow::newFile);
    actionsArr_[k_TYPE_NEW] = newAction_;

    settingsAction_ = new QAction(tr("Settings"), this);
    //    saveAllAction_->setShortcuts(QKeySequence::New);
    settingsAction_->setStatusTip(tr("Settings"));
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::settingsWindow);

    saveAction_ = new QAction(tr("Save"), this);
    saveAction_->setShortcut(QKeySequence(settings->shortcut("TYPE_SAVE")));
    saveAction_->setStatusTip(tr("Save"));
    connect(saveAction_, &QAction::triggered, this, &MainWindow::saveFile);
    actionsArr_[k_TYPE_SAVE] = saveAction_;

    quitAction_ = new QAction(tr("Quit"), this);
    quitAction_->setShortcut(QKeySequence(settings->shortcut("TYPE_QUIT")));
    quitAction_->setStatusTip(tr("Quit"));
    connect(quitAction_, &QAction::triggered, this, &MainWindow::quit);
    actionsArr_[k_TYPE_QUIT] = quitAction_;

    openAction_ = new QAction(tr("Open"), this);
    openAction_->setShortcut(QKeySequence(settings->shortcut("TYPE_OPEN")));
    openAction_->setStatusTip(tr("Open"));
    connect(openAction_, &QAction::triggered, this, &MainWindow::openFile);
    actionsArr_[k_TYPE_OPEN] = openAction_;

    saveAsAction_ = new QAction(tr("Save As"), this);
    //    saveAllAction_->setShortcuts(QKeySequence::New);
    saveAsAction_->setStatusTip(tr("Save As"));
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::saveFileAs);
}

void MainWindow::createMenus()
{
    fileMenu_ = menuBar()->addMenu(tr("menu"));
    fileMenu_->setStyleSheet("background: transparent; background-color: rgba(0, 255, 0, 255);");
    menuBar()->setStyleSheet("background: transparent; background-color: rgba(0, 255, 0, 255);");
    fileMenu_->addAction(newAction_);
    fileMenu_->addAction(openAction_);
    fileMenu_->addAction(saveAction_);
    fileMenu_->addAction(saveAsAction_);
    fileMenu_->addAction(saveAllAction_);
    fileMenu_->addAction(settingsAction_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(quitAction_);
}


void MainWindow::saveAllWindowSaveCB(SaveAllWindow* w, std::map<int, bool>&& m)
{
    w->close();

    int exit_flag = 1;

    for (auto it = m.rbegin(); it != m.rend(); it++) {
        if (!it->second) {
            qDebug() << "close ID = " << it->first << " " << tabpane_->getCurrentTabPath();
            tabpane_->closeTabByIndex(it->first);
        }
    }

    for (int i = tabpane_->count() - 1; i >= 0; --i) {
        qDebug() << "save ID = " << i << " " << tabpane_->getCurrentTabPath();
        tabpane_->setCurrentIndex(i);

        auto ret = fileactions_->saveFile();
        if (ret == QDialog::Rejected) {
            exit_flag = 0;
        } else {
            tabpane_->closeTabByIndex(i);
        }
    }

    if (exit_flag)
        exitProject();
}

void MainWindow::cleanupWorkplace()
{
    //    canvasWidget->cleanupWorkplace();
}

void MainWindow::exitProject()
{
    qApp->exit(0); // Is it correct way?
}

TabPane& MainWindow::tabPane()
{
    return *tabpane_;
}

FileActions& MainWindow::fileActions()
{
    return *fileactions_;
}


void MainWindow::closeEvent(QCloseEvent* event)
{
    if (checkSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    qreal opacity = (qreal)currentOpacity_/255;
    painter.setOpacity(opacity);
    painter.fillRect(
        event->rect(),
        backGroundColor_); // тут поменяем цвет из настроек и сделаем доп функцию где будем менять опасити
    // Нарисуйте другие элементы интерфейса здесь
    //QMainWindow::paintEvent(event); // Вызов базовой реализации
}

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
{
    ui->setupUi(this);
    // tabpane_ must be constructed after ui->setupUi(this): building it
    // constructs the first CanvasView, which builds a real, populated
    // menu bar via mainwindow_.setMenuBar(...). setupUi() itself calls
    // setMenuBar() with its own (empty) Designer-generated QMenuBar - if
    // tabpane_ were built first (e.g. via the member-initializer list,
    // which runs before this constructor body), setupUi() would silently
    // discard the real menu bar right after it was set.
    tabpane_ = new TabPane(this, *this);
    setWindowTitle("Familiar");
    statusBar()->hide();

    setMouseTracking(true);
    //this->setWindowFlags(Qt::WindowTransparentForInput|Qt::WindowStaysOnTopHint);

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
    setMouseTracking(true);

    // Central widget (tabpane_) covers the whole frameless window, so this
    // watches every mouse move to keep the border-resize cursor in sync.
    qApp->installEventFilter(this);
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
    SettingsWindow* widget = new SettingsWindow(this, this->parentWidget());

    // widget->raise();
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

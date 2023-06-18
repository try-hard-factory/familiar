#include "tabpane.h"
#include "canvasview.h"
#include "fml_file_buffer.h"
#include "mainwindow.h"
#include "project_settings.h"
#include <QFileInfo>

     
TabPane::TabPane(QWidget* parent, MainWindow& mw)
    : QWidget(parent), mainwindow_(mw)
{
    layout_ = new QVBoxLayout; // try some other layout
    layout_->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout_);

    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(true);
    tabs_->setWindowFlags(Qt::FramelessWindowHint);
    tabs_->setAttribute(Qt::WA_TranslucentBackground);
    // // tabs_->setStyleSheet("background: transparent; background-color: rgba(255, 255, 255, 128);");
    layout_->addWidget(tabs_);

    addNewUntitledTab();
    
    // // setStyleSheet("background: transparent; background-color: rgba(0, 0, 0, 128);");
    connect(tabs_, SIGNAL(tabCloseRequested(int)), this, SLOT(onTabClosed(int)));
}

TabPane::~TabPane()
{
    delete tabs_;
    delete layout_;
}

// void TabPane::paintEvent(QPaintEvent* event)
// {
//     QPainter painter(this);
//     painter.setOpacity(0.6);
//     painter.fillRect(event->rect(), Qt::black);// тут поменяем цвет из настроек и сделаем доп функцию где будем менять опасити
//     // Нарисуйте другие элементы интерфейса здесь
//     //QWidget::paintEvent(event); // Вызов базовой реализации
// }

void TabPane::addNewTab(const QString& path)
{
    int count = tabs_->count();

    CanvasView* canvasView = new CanvasView(mainwindow_);
    project_settings* ps = new project_settings(this);

    ps->path(path);
    ps->projectName(QFileInfo(path).fileName());
    canvasView->setProjectSettings(ps);
    canvasView->show();

    tabs_->addTab(canvasView, QFileInfo(path).fileName());
    tabs_->setCurrentIndex(count);
}

void TabPane::closeTabByIndex(int idx)
{
    tabs_->removeTab(idx);
}

void TabPane::addNewUntitledTab()
{
    int count = tabs_->count();

    CanvasView* canvasWidget = new CanvasView(mainwindow_);
    project_settings* ps = new project_settings(this);
    canvasWidget->setProjectSettings(ps);
    canvasWidget->show();

    tabs_->addTab(canvasWidget, "untitled");
    tabs_->setCurrentIndex(count);
}

void TabPane::setCurrentTabPath(const QString& path)
{
    currentWidget()->setPath(path);
}

QString TabPane::getCurrentTabPath()
{
    return currentWidget()->path();
}

void TabPane::onTabClosed(int index)
{
    CanvasView* canvasview = widgetAt(index);
    if (canvasview->isModified()) {
        QMessageBox::StandardButton resBtn = QMessageBox::warning(
            this,
            "Warning!",
            tr("You have unsaved documents!\n\nDo you want to save it?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);

        if (resBtn == QMessageBox::Yes) {
            if (mainwindow_.fileActions().saveFile() == QDialog::Accepted) {
                delete canvasview;
                if (tabs_->count() == 0) {
                    addNewUntitledTab();
                }
            }
        } else if (resBtn == QMessageBox::No) {
            delete canvasview;
            if (tabs_->count() == 0) {
                addNewUntitledTab();
            }
        }
    } else {
        delete canvasview;
        if (tabs_->count() == 0) {
            addNewUntitledTab();
        }
    }
}


void TabPane::setCurrentTabTitle(const QString& title)
{
    tabs_->setTabText(tabs_->currentIndex(), title);
}

QString TabPane::getCurrentTabTitle()
{
    return tabs_->tabText(tabs_->currentIndex());
}

void TabPane::setCurrentTabProjectName(const QString& pn)
{
    currentWidget()->setProjectName(pn);
}

QString TabPane::getCurrentTabProjectName()
{
    return currentWidget()->projectName();
}

CanvasView* TabPane::currentWidget()
{
    return static_cast<CanvasView*>(tabs_->currentWidget());
}

CanvasView* TabPane::widgetAt(int index)
{
    return static_cast<CanvasView*>(tabs_->widget(index));
}

void TabPane::setCurrentIndex(int index)
{
    tabs_->setCurrentIndex(index);
}

int TabPane::count()
{
    return tabs_->count();
}

#include "tabpane.h"
#include "canvasview.h"
#include "mainwindow.h"
#include "project_settings.h"
#include "recovery.h"
#include "widgets/message_box.h"
#include <QFileInfo>
#include <QMessageBox>


TabPane::TabPane(QWidget* parent, MainWindow& mw)
    : QWidget(parent)
    , mainwindow_(mw)
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
    connect(tabs_,
            &QTabWidget::currentChanged,
            this,
            &TabPane::currentTabChanged);
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
    project_settings* ps = new project_settings(this, canvasView);

    ps->path(path);
    ps->projectName(QFileInfo(path).fileName());
    canvasView->setProjectSettings(ps);
    canvasView->show();

    tabs_->addTab(canvasView, QFileInfo(path).fileName());
    setCloseButtonTooltip_(count);
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
    project_settings* ps = new project_settings(this, canvasWidget);
    canvasWidget->setProjectSettings(ps);
    canvasWidget->show();

    tabs_->addTab(canvasWidget, "untitled");
    setCloseButtonTooltip_(count);
    tabs_->setCurrentIndex(count);
}

void TabPane::setCloseButtonTooltip_(int index)
{
    // The close button can be docked on either side depending on the
    // active style, so try both rather than assuming RightSide.
    if (QWidget* btn = tabs_->tabBar()->tabButton(index, QTabBar::RightSide)) {
        btn->setToolTip(tr("Close project"));
    }
    if (QWidget* btn = tabs_->tabBar()->tabButton(index, QTabBar::LeftSide)) {
        btn->setToolTip(tr("Close project"));
    }
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
    // This tab's fate (saved or explicitly discarded) is being decided
    // right now by the branches below - whatever they choose, a stale
    // recovery snapshot from earlier in this session shouldn't linger
    // and falsely offer to "recover" an already-closed tab after some
    // later crash in the same run (see recovery.h).
    familiar::recovery::remove(canvasview->recoveryId());
    if (canvasview->isModified()) {
        QMessageBox::StandardButton resBtn = showMessageBox(
            QMessageBox::Warning,
            this,
            tr("Warning!"),
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

void TabPane::setTabTitle(CanvasView* view, const QString& title)
{
    const int idx = tabs_->indexOf(view);
    if (idx >= 0) {
        tabs_->setTabText(idx, title);
    }
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

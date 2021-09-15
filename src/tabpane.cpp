#include <QFileInfo>
#include "tabpane.h"
#include "mainwindow.h"
#include "canvasview.h"
#include "project_settings.h"
#include "fml_file_buffer.h"

TabPane::TabPane(MainWindow &mw) : mainwindow_(mw)
{
    layout_ = new QVBoxLayout; // try some other layout
    layout_->setContentsMargins(0,0,0,0);
    this->setLayout(layout_);

    tabs_ = new QTabWidget();
    tabs_->setTabsClosable(true);
    layout_->addWidget(tabs_);

    addNewUntitledTab();

    connect(tabs_,SIGNAL(tabCloseRequested(int)), this, SLOT(onTabClosed(int)));
}

TabPane::~TabPane()
{
    delete tabs_;
    delete layout_;
}

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

void TabPane::addNewUntitledTab() {
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

void TabPane::onTabClosed(int index) {
    CanvasView* canvasview = widgetAt(index);
    if (canvasview->isModified()) {
        QMessageBox::StandardButton resBtn = QMessageBox::warning( this, "Warning!",
                                                                    tr("You have unsaved documents!\n\nDo you want to save it?"),
                                                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel ,
                                                                    QMessageBox::No);

        if (resBtn == QMessageBox::Yes) {
            if (mainwindow_.fileActions().saveFile() == QDialog::Accepted) {
                delete canvasview;
                if (tabs_->count()==0) {
                    addNewUntitledTab();
                }
            }
        } else if (resBtn == QMessageBox::No) {
            delete canvasview;
            if (tabs_->count()==0) {
                addNewUntitledTab();
            }
        }
    } else {
        delete canvasview;
        if (tabs_->count()==0) {
            addNewUntitledTab();
        }
    }
}


void TabPane::setCurrentTabTitle(const QString& title)
{
    tabs_->setTabText(tabs_->currentIndex(),title);
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

CanvasView *TabPane::currentWidget()
{
    return static_cast<CanvasView *>(tabs_->currentWidget());
}

CanvasView *TabPane::widgetAt(int index)
{
    return static_cast<CanvasView *>(tabs_->widget(index));
}

void TabPane::setCurrentIndex(int index)
{
    tabs_->setCurrentIndex(index);
}

int TabPane::count()
{
    return tabs_->count();
}

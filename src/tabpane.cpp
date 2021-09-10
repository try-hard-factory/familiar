#include <QFileInfo>
#include "tabpane.h"
#include "mainwindow.h"
#include "canvasview.h"
#include "project_settings.h"
#include "fml_file_buffer.h"

TabPane::TabPane(FileActions& fa) : fileActions_(fa)
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

void TabPane::addNewTab(QString path)
{
    int count = tabs_->count();

    CanvasView* canvasView = new CanvasView(fileActions_.mainWindow());
    project_settings* ps = new project_settings(this);

    ps->path(path);
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

    CanvasView* canvasWidget = new CanvasView(fileActions_.mainWindow());
    project_settings* ps = new project_settings(this);
    canvasWidget->setProjectSettings(ps);
    canvasWidget->show();

    tabs_->addTab(canvasWidget, "untitled");
    tabs_->setCurrentIndex(count);
}

void TabPane::setCurrentTabPath(QString path)
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
            if (fileActions_.saveFile() == QDialog::Accepted) {
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


void TabPane::setCurrentTabTitle(QString title)
{
    tabs_->setTabText(tabs_->currentIndex(),title);
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

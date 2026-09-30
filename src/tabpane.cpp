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

    add_new_untitled_tab();

    // // setStyleSheet("background: transparent; background-color: rgba(0, 0, 0, 128);");
    connect(tabs_, SIGNAL(tabCloseRequested(int)), this, SLOT(onTabClosed(int)));
    connect(tabs_,
            &QTabWidget::currentChanged,
            this,
            &TabPane::current_tab_changed);
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

void TabPane::add_new_tab(const QString& path)
{
    int count = tabs_->count();

    // CanvasView(MainWindow&, QWidget* parent = nullptr) - canvasView has
    // NO parent yet at this point, so calling show() here (like this
    // used to) shows it as a genuine standalone top-level OS window for
    // the one frame before tabs_->addTab() below reparents it into the
    // tab stack - visible as a real separate window flashing on screen
    // (default title bar/background, no frameless/translucent styling)
    // whenever a tab is created, confirmed on Windows. QTabWidget
    // handles showing/hiding its pages itself once they're actually
    // added - no need to show() a widget that isn't parented into
    // anything real yet.
    CanvasView* canvasView = new CanvasView(mainwindow_);
    ProjectSettings* ps = new ProjectSettings(this, canvasView);

    ps->path(path);
    ps->project_name(QFileInfo(path).fileName());
    canvasView->set_project_settings(ps);

    tabs_->addTab(canvasView, QFileInfo(path).fileName());
    set_close_button_tooltip(count);
    tabs_->setCurrentIndex(count);
}

void TabPane::close_tab_by_index(int idx)
{
    tabs_->removeTab(idx);
}

void TabPane::add_new_untitled_tab()
{
    int count = tabs_->count();

    // No premature show() here either - see addNewTab()'s own comment.
    CanvasView* canvasWidget = new CanvasView(mainwindow_);
    ProjectSettings* ps = new ProjectSettings(this, canvasWidget);
    canvasWidget->set_project_settings(ps);

    tabs_->addTab(canvasWidget, "untitled");
    set_close_button_tooltip(count);
    tabs_->setCurrentIndex(count);
}

void TabPane::set_close_button_tooltip(int index)
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

void TabPane::set_current_tab_path(const QString& path)
{
    current_widget()->set_path(path);
}

QString TabPane::get_current_tab_path()
{
    return current_widget()->path();
}

void TabPane::on_tab_closed(int index)
{
    CanvasView* canvasview = widget_at(index);
    // This tab's fate (saved or explicitly discarded) is being decided
    // right now by the branches below - whatever they choose, a stale
    // recovery snapshot from earlier in this session shouldn't linger
    // and falsely offer to "recover" an already-closed tab after some
    // later crash in the same run (see recovery.h).
    familiar::recovery::remove(canvasview->recovery_id());
    if (canvasview->is_modified()) {
        QMessageBox::StandardButton resBtn = show_message_box(
            QMessageBox::Warning,
            this,
            tr("Warning!"),
            tr("You have unsaved documents!\n\nDo you want to save it?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);

        if (resBtn == QMessageBox::Yes) {
            if (mainwindow_.file_actions().save_file() == QDialog::Accepted) {
                delete canvasview;
                if (tabs_->count() == 0) {
                    add_new_untitled_tab();
                }
            }
        } else if (resBtn == QMessageBox::No) {
            delete canvasview;
            if (tabs_->count() == 0) {
                add_new_untitled_tab();
            }
        }
    } else {
        delete canvasview;
        if (tabs_->count() == 0) {
            add_new_untitled_tab();
        }
    }
}


void TabPane::set_current_tab_title(const QString& title)
{
    tabs_->setTabText(tabs_->currentIndex(), title);
}

void TabPane::set_tab_title(CanvasView* view, const QString& title)
{
    const int idx = tabs_->indexOf(view);
    if (idx >= 0) {
        tabs_->setTabText(idx, title);
    }
}

QString TabPane::get_current_tab_title()
{
    return tabs_->tabText(tabs_->currentIndex());
}

void TabPane::set_current_tab_project_name(const QString& pn)
{
    current_widget()->set_project_name(pn);
}

QString TabPane::get_current_tab_project_name()
{
    return current_widget()->project_name();
}

CanvasView* TabPane::current_widget()
{
    return static_cast<CanvasView*>(tabs_->currentWidget());
}

CanvasView* TabPane::widget_at(int index)
{
    return static_cast<CanvasView*>(tabs_->widget(index));
}

void TabPane::set_current_index(int index)
{
    tabs_->setCurrentIndex(index);
}

int TabPane::count()
{
    return tabs_->count();
}

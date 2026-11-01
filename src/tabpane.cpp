#include "tabpane.h"
#include "canvasview.h"
#include "mainwindow.h"
#include "project_settings.h"
#include "recovery.h"
#include "widgets/message_box.h"
#include <QArgument>
#include <QBoxLayout>
#include <QDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QObject>
#include <QTabWidget>
#include <QWidget>
#include <Qt>


TabPane::TabPane(QWidget* parent, MainWindow& mw)
    : QWidget(parent)
    , mainwindow_(mw)
    , layout_(new QVBoxLayout)
    , tabs_(new QTabWidget(this))
{
    // try some other layout
    layout_->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout_);


    tabs_->setTabsClosable(true);
    tabs_->setWindowFlags(Qt::FramelessWindowHint);
    tabs_->setAttribute(Qt::WA_TranslucentBackground);
    // // tabs_->setStyleSheet("background: transparent; background-color: rgba(255, 255, 255, 128);");
    layout_->addWidget(tabs_);

    add_new_untitled_tab();

    // // setStyleSheet("background: transparent; background-color: rgba(0, 0, 0, 128);");
    connect(tabs_,
            &QTabWidget::tabCloseRequested,
            this,
            &TabPane::on_tab_closed);
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
    const int count = tabs_->count();

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
    auto* canvasView = new CanvasView(mainwindow_);
    auto* ps = new ProjectSettings(this, canvasView);

    ps->path(path);
    ps->project_name(QFileInfo(path).fileName());
    canvasView->set_project_settings(ps);

    tabs_->addTab(canvasView, QFileInfo(path).fileName());
    set_close_button_tooltip(count);
    tabs_->setCurrentIndex(count);
}

void TabPane::close_tab_by_index(int idx)
{
    // Deleting the page, not removeTab(): the latter only detaches the
    // widget from the tab bar and leaves it parentless and alive, so the
    // whole CanvasView - its scene, every picture in it - leaked. Nothing
    // reclaimed it, and this is also reachable without the app then
    // exiting (MainWindow::save_all_window_save_cb() stays open if a save
    // is cancelled), so it was a leak in a live session, not just on the
    // way out. QTabWidget notices the page being destroyed and drops its
    // tab entry on its own - same direct `delete` FileActions::
    // close_tab() and TabPane::on_tab_closed() already rely on.
    // widget() returns nullptr for an out-of-range index, and deleting
    // that is a no-op.
    delete tabs_->widget(idx);
}

int TabPane::index_of(CanvasView* view) const
{
    return tabs_->indexOf(view);
}

void TabPane::add_new_untitled_tab()
{
    const int count = tabs_->count();

    // No premature show() here either - see addNewTab()'s own comment.
    auto* canvasWidget = new CanvasView(mainwindow_);
    auto* ps = new ProjectSettings(this, canvasWidget);
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
    if (canvasview->is_modified()) {
        // Activated BEFORE the prompt, for correctness rather than
        // cosmetics: FileActions::save_file() saves current_widget(), and
        // so does save_file_as() for an untitled tab (see its own comment
        // in file_actions.h). Closing a BACKGROUND tab therefore used to
        // write whichever document happened to be in front, then delete
        // the one the user had just agreed to save - its changes gone,
        // and the active tab silently overwritten on top. Showing the tab
        // being asked about is also the honest thing to do: the prompt
        // says "you have unsaved documents" without naming which.
        set_current_index(index);

        const QMessageBox::StandardButton resBtn = show_message_box(
            QMessageBox::Warning,
            this,
            tr("Warning!"),
            tr("You have unsaved documents!\n\nDo you want to save it?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);

        if (resBtn == QMessageBox::Yes) {
            if (mainwindow_.file_actions().save_file() != QDialog::Accepted) {
                return; // save failed, or its Save As dialog was cancelled
            }
        } else if (resBtn != QMessageBox::No) {
            return; // Cancel, or the prompt dismissed outright
        }
    }

    // Only here is this tab's fate actually settled - saved, or
    // explicitly discarded. Removing the recovery snapshot any earlier
    // (it used to happen before the prompt) threw away the only copy of
    // the unsaved work in exactly the cases where it still mattered: the
    // user pressing Cancel, or the save failing and the tab staying open.
    // Once the tab really is going, the snapshot has to go with it, or a
    // later crash in this same session would offer to "recover" a tab
    // that was deliberately closed (see recovery.h).
    familiar::recovery::remove(canvasview->recovery_id());
    delete canvasview;
    if (tabs_->count() == 0) {
        add_new_untitled_tab();
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
    return qobject_cast<CanvasView*>(tabs_->currentWidget());
}

CanvasView* TabPane::widget_at(int index)
{
    return qobject_cast<CanvasView*>(tabs_->widget(index));
}

void TabPane::set_current_index(int index)
{
    tabs_->setCurrentIndex(index);
}

int TabPane::count()
{
    return tabs_->count();
}

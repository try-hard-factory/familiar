#include "welcome_overlay.h"
#include "canvasview.h"
#include "log/log.h"
#include "mainwindow.h"
#include <core/settingshandler.h>
#include <QLabel>
#include <QVBoxLayout>
using namespace familiar::log;

WelcomeOverlay::WelcomeOverlay(QWidget* parent, MainWindow* mainWindow)
    : MainControlsMixin<WelcomeOverlay, QWidget>(parent)
    , mainWindow_(mainWindow)
    , filesWidget_(new QWidget(this))
    , layout_(new QHBoxLayout(this))
{
    setAutoFillBackground(true);
    set_control_target(parent);
    init_main_controls(mainWindow);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    // Recent files widget (hidden until there are recent files)

    auto* filesLayout = new QVBoxLayout(filesWidget_);
    filesLayout->addStretch(50);
    filesLayout->addWidget(new QLabel(QStringLiteral("<h3>Recent Files</h3>")));
    filesView_ = new RecentFilesView(this, {}, mainWindow_);
    filesLayout->addWidget(filesView_);
    filesLayout->addStretch(50);
    filesWidget_->hide();

    // Help text (always visible, transparent to mouse so WelcomeOverlay stays the grabber)
    label_ = new QLabel(ktxt, this);
    label_->setAlignment(Qt::AlignVCenter | Qt::AlignCenter);
    label_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    layout_->addStretch(50);
    layout_->addWidget(label_);
    layout_->addStretch(50);
}

void WelcomeOverlay::show()
{
    const QStringList files = SettingsHandler::get_recent_files(true);
    filesView_->update_files(files);
    if (!files.isEmpty()) {
        if (layout_->indexOf(filesWidget_) < 0) {
            layout_->insertWidget(0, filesWidget_);
        }
        filesWidget_->show();
    } else if (layout_->indexOf(filesWidget_) >= 0) {
        // Mirror image of the branch above - a real bug this fixes: this
        // widget was only ever ADDED+shown once files stopped being
        // empty, never removed+hidden again once it WAS added and files
        // becomes empty later (e.g. this tab's recent-files snapshot
        // just happened to be empty on THIS particular show() call) -
        // left a bare "Recent Files" heading with nothing under it
        // visible instead of just not showing this widget at all.
        layout_->removeWidget(filesWidget_);
        filesWidget_->hide();
    }
    QWidget::show();
}

void WelcomeOverlay::disable_mouse_events()
{
    filesView_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    label_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void WelcomeOverlay::enable_mouse_events()
{
    filesView_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    label_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void WelcomeOverlay::on_context_menu(const QPoint& point)
{
    qobject_cast<CanvasView*>(parent())->on_context_menu(point);
}

void WelcomeOverlay::mousePressEvent(QMouseEvent* event)
{
    if (mouse_press_event_main_controls(event)) {
        return;
    }
    QWidget::mousePressEvent(event);
}

void WelcomeOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (mouse_move_event_main_controls(event)) {
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void WelcomeOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (mouse_release_event_main_controls(event)) {
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void WelcomeOverlay::keyPressEvent(QKeyEvent* event)
{
    if (key_press_event_main_controls(event)) {
        return;
    }
    QWidget::keyPressEvent(event);
}

void WelcomeOverlay::dropEvent(QDropEvent* event)
{
    FLOG_DEBUG(Ch::UI, "WelcomeOverlay::Handling file drop:");
    if (auto* canvas = qobject_cast<CanvasView*>(parent())) {
        const QPoint pos(qRound(event->position().x()),
                         qRound(event->position().y()));
        canvas->handle_drop(event->mimeData(), pos);
    }
    event->acceptProposedAction();
}
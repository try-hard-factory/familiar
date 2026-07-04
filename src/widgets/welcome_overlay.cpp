#include "welcome_overlay.h"
#include "mainwindow.h"
#include <core/settings.h>
#include <QLabel>
#include <QVBoxLayout>

WelcomeOverlay::WelcomeOverlay(QWidget* parent, MainWindow* mainWindow)
    : MainControlsMixin<WelcomeOverlay, QWidget>(parent)
    , mainWindow_(mainWindow)
{
    setAutoFillBackground(true);
    init_main_controls(mainWindow);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    // Recent files widget (hidden until there are recent files)
    filesWidget_ = new QWidget(this);
    auto* filesLayout = new QVBoxLayout(filesWidget_);
    filesLayout->addStretch(50);
    filesLayout->addWidget(new QLabel(QStringLiteral("<h3>Recent Files</h3>")));
    filesView_ = new RecentFilesView(this, {});
    filesLayout->addWidget(filesView_);
    filesLayout->addStretch(50);
    filesWidget_->hide();

    // Help text (always visible, transparent to mouse so WelcomeOverlay stays the grabber)
    label_ = new QLabel(txt, this);
    label_->setAlignment(Qt::AlignVCenter | Qt::AlignCenter);
    label_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layout_ = new QHBoxLayout(this);
    layout_->addStretch(50);
    layout_->addWidget(label_);
    layout_->addStretch(50);
}

void WelcomeOverlay::show()
{
    QStringList files = FamSettings().getRecentFiles(true);
    filesView_->update_files(files);
    if (!files.isEmpty() && layout_->indexOf(filesWidget_) < 0) {
        layout_->insertWidget(0, filesWidget_);
        filesWidget_->show();
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
    static_cast<CanvasView*>(parent())->on_context_menu(point);
}

void WelcomeOverlay::mousePressEvent(QMouseEvent* event)
{
    if (mousePressEventMainControls(event))
        return;
    QWidget::mousePressEvent(event);
}

void WelcomeOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (mouseMoveEventMainControls(event))
        return;
    QWidget::mouseMoveEvent(event);
}

void WelcomeOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (mouseReleaseEventMainControls(event))
        return;
    QWidget::mouseReleaseEvent(event);
}

void WelcomeOverlay::keyPressEvent(QKeyEvent* event)
{
    if (keyPressEventMainControls(event))
        return;
    QWidget::keyPressEvent(event);
}

#pragma once

#include "main_controls.h"
#include "recent_files_view.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPoint>
#include <QUrl>
#include <QWidget>

class MainWindow;

class WelcomeOverlay : public MainControlsMixin<WelcomeOverlay, QWidget>
{
    Q_OBJECT
public:
    explicit WelcomeOverlay(QWidget* parent, MainWindow* mainWindow = nullptr);

    void show();
    void disable_mouse_events();
    void enable_mouse_events();
    void do_insert_images(const QList<QUrl>&, const QPoint&) {}

public slots:
    // void on_context_menu(const QPoint& point);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    static constexpr char txt[] = R"(
        <p>Paste or drop images here.</p>
        <p>Right-click for more options.</p>
    )";

    MainWindow*      mainWindow_;
    QWidget*         filesWidget_;
    RecentFilesView* filesView_;
    QLabel*          label_;
    QHBoxLayout*     layout_;
};

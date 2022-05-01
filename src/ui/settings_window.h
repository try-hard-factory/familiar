#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QTabWidget>
#include <QWidget>

class MainWindow;
class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(MainWindow* wm, QWidget *parent = nullptr);

private:
    MainWindow* window_ = nullptr;

    QTabWidget* tabWidget_ = nullptr;
};

#endif // SETTINGSWINDOW_H

#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QWidget>

class MainWindow;
class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(MainWindow* wm, QWidget *parent = nullptr);

private:
    MainWindow* window_;
};

#endif // SETTINGSWINDOW_H

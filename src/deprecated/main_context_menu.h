#ifndef MAINCONTEXTMENU_H
#define MAINCONTEXTMENU_H

#include <QMenu>

class MainWindow;

class MainContextMenu : public QMenu
{
    Q_OBJECT
public:
    MainContextMenu(MainWindow& wm, QWidget* parent = nullptr);
    ~MainContextMenu();
private slots:
    void quit();

private:
    MainWindow& mainwindow_;
    QAction* quit_action = nullptr;
};

#endif // MAINCONTEXTMENU_H

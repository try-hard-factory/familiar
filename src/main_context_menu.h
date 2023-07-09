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

public slots:
    void settingsChangedSlot();
private slots:
    void settings();
    void quit();
    void note();

private:
    MainWindow& mainwindow_;
    QAction* quitAction_ = nullptr;
    QAction* settingsAction_ = nullptr;
    QAction* noteAction_ = nullptr;
    QColor menuColor_;
};

#endif // MAINCONTEXTMENU_H

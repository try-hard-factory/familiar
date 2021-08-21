#ifndef TABPANE_H
#define TABPANE_H

#include <QVBoxLayout>
#include <QFrame>
#include <QTabWidget>

class MainWindow;
class CanvasView;
class FileActions;
class TabPane : public QFrame
{
    Q_OBJECT
public:
    TabPane(FileActions& fa);
    ~TabPane();

    void addNewTab(QString path);
    void addNewUntitledTab();
    void setCurrentTabPath(QString path);
    void setCurrentTabTitle(QString title);
    CanvasView* currentWidget();
    CanvasView* widgetAt(int index);
    void setCurrentIndex(int index);
    int count();

private slots:
    void onTabClosed(int index);

private:
//    MainWindow* window_;
    FileActions& fileActions_;
    QVBoxLayout* layout_;
    QTabWidget* tabs_;
};

#endif // TABPANE_H

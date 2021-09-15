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
    explicit TabPane(MainWindow& mw);
    ~TabPane();

    void addNewTab(const QString& path);
    void closeTabByIndex(int idx);
    void addNewUntitledTab();

    void setCurrentTabPath(const QString& path);
    QString getCurrentTabPath();

    void setCurrentTabTitle(const QString& title);
    QString getCurrentTabTitle();

    void setCurrentTabProjectName(const QString& pn);
    QString getCurrentTabProjectName();

    CanvasView* currentWidget();
    CanvasView* widgetAt(int index);
    void setCurrentIndex(int index);
    int count();

private slots:
    void onTabClosed(int index);

private:
    MainWindow& mainwindow_;
    QVBoxLayout* layout_;
    QTabWidget* tabs_;
};

#endif // TABPANE_H

#ifndef TABPANE_H
#define TABPANE_H

#include <QFrame>
#include <QTabWidget>
#include <QVBoxLayout>

class MainWindow;
class CanvasView;
class FileActions;
class TabPane : public QWidget
{
    Q_OBJECT
public:
    explicit TabPane(QWidget* parent, MainWindow& mw);
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

signals:
    // Forwards the internal QTabWidget's currentChanged(int), so
    // MainWindow can resync its shared action enabled-state to whichever
    // tab is now active (see MainWindow::resyncActionsForTab).
    void currentTabChanged(int index);

protected:
    // void paintEvent(QPaintEvent* event) override;

private slots:
    void onTabClosed(int index);

private:
    MainWindow& mainwindow_;
    QVBoxLayout* layout_;
    QTabWidget* tabs_;
};

#endif // TABPANE_H

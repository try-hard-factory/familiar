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
    void setTabTitle(CanvasView* view, const QString& title);

    void setCurrentTabProjectName(const QString& pn);
    QString getCurrentTabProjectName();

    CanvasView* currentWidget();
    CanvasView* widgetAt(int index);
    void setCurrentIndex(int index);
    int count();

    // The internal QTabWidget's tab bar - MainWindow fades it together
    // with the menu bar in auto-hide-UI mode.
    QTabBar* tabBar() const { return tabs_->tabBar(); }

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
    // Qt's native close-button tooltip just says "Close Tab", which is
    // misleading here - a tab is a whole loaded .fml project, not a
    // lightweight document tab. Overridden per-tab since QTabBar has no
    // single stylesheet/property for it.
    void setCloseButtonTooltip_(int index);


    MainWindow& mainwindow_;
    QVBoxLayout* layout_;
    QTabWidget* tabs_;
};

#endif // TABPANE_H

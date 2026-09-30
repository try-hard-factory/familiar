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

    void add_new_tab(const QString& path);
    void close_tab_by_index(int idx);
    void add_new_untitled_tab();

    void set_current_tab_path(const QString& path);
    QString get_current_tab_path();

    void set_current_tab_title(const QString& title);
    QString get_current_tab_title();
    void set_tab_title(CanvasView* view, const QString& title);

    void set_current_tab_project_name(const QString& pn);
    QString get_current_tab_project_name();

    CanvasView* current_widget();
    CanvasView* widget_at(int index);
    void set_current_index(int index);
    int count();

    // The internal QTabWidget's tab bar - MainWindow fades it together
    // with the menu bar in auto-hide-UI mode.
    QTabBar* tab_bar() const { return tabs_->tabBar(); }

signals:
    // Forwards the internal QTabWidget's currentChanged(int), so
    // MainWindow can resync its shared action enabled-state to whichever
    // tab is now active (see MainWindow::resyncActionsForTab).
    void current_tab_changed(int index);

protected:
    // void paintEvent(QPaintEvent* event) override;

private slots:
    void on_tab_closed(int index);

private:
    // Qt's native close-button tooltip just says "Close Tab", which is
    // misleading here - a tab is a whole loaded .fml project, not a
    // lightweight document tab. Overridden per-tab since QTabBar has no
    // single stylesheet/property for it.
    void set_close_button_tooltip(int index);


    MainWindow& mainwindow_;
    QVBoxLayout* layout_;
    QTabWidget* tabs_;
};

#endif // TABPANE_H

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
    ~TabPane() override;
    // QObject identity is not copyable - QObject itself declares
    // Q_DISABLE_COPY, so copies were already impossible; this states it
    // on the class that has the destructor, which is what the rule of
    // five (and cppcoreguidelines-special-member-functions) asks for.
    Q_DISABLE_COPY_MOVE(TabPane)

    void add_new_tab(const QString& path);
    void close_tab_by_index(int idx);

    // Current index of `view`, or -1 if it is not one of the pages.
    // Re-derived on demand rather than cached: closing any tab shifts
    // every index after it, so an index captured earlier may already mean
    // a different tab (see MainWindow::save_all_window_save_cb()).
    int index_of(CanvasView* view) const;
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

    // Closes the slots/signals section above - moc needs it, even
    // though to the compiler it repeats the enclosing access level.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
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

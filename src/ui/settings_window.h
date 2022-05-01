#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QTabWidget>
#include <QWidget>

class MainWindow;
class ShortcutsWidget;

class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(MainWindow* wm, QWidget *parent = nullptr);

signals:
    void updateChildren();

protected:
    void keyPressEvent(QKeyEvent*);

private:
    MainWindow* window_ = nullptr;

    QTabWidget* tabWidget_ = nullptr;

    // VisualsEditor* visuals_ = nullptr;
    QWidget* visualsTab_ = nullptr;

    // PreferencesConf* prefConfig_ = nullptr;
    QWidget* prefConfigTab_ = nullptr;

    ShortcutsWidget* shortcuts_ = nullptr;
    QWidget* shortcutsTab_ = nullptr;
};

#endif // SETTINGSWINDOW_H

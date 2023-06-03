#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QTabWidget>
#include <QWidget>

class MainWindow;
class PreferencesConf;
class ColorsWidget;
class ShortcutsWidget;

class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(MainWindow* wm, QWidget* parent = nullptr);

signals:
    void updateChildren();

protected:
    void keyPressEvent(QKeyEvent*);

private:
    MainWindow* window_ = nullptr;
    QTabWidget* tabWidget_ = nullptr;
    PreferencesConf* prefConfig_ = nullptr;
    QWidget* prefConfigTab_ = nullptr;
    ColorsWidget* colors_ = nullptr;
    QWidget* colorsTab_ = nullptr;
    ShortcutsWidget* shortcuts_ = nullptr;
    QWidget* shortcutsTab_ = nullptr;
};

#endif // SETTINGSWINDOW_H

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
private:

signals:
    void updateChildren();

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    MainWindow* window_ = nullptr;
    QTabWidget* tabWidget_ = nullptr;
    QWidget* prefConfigTab_ = nullptr;
    PreferencesConf* prefConfig_ = nullptr;
    QWidget* colorsTab_ = nullptr;
    ColorsWidget* colors_ = nullptr;
    QWidget* shortcutsTab_ = nullptr;
    ShortcutsWidget* shortcuts_ = nullptr;
};

#endif // SETTINGSWINDOW_H

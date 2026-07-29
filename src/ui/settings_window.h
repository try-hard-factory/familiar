#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QTabWidget>
#include <QWidget>

class MainWindow;
class ColorsWidget;
class ConfirmCloseUnsavedWidget;
class ImageStorageFormatWidget;
class ArrangeGapWidget;
class AllocationLimitWidget;
class ArrangeDefaultWidget;
class KeyboardShortcutsView;
class MouseView;
class MouseWheelView;

class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(MainWindow* wm, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    MainWindow* window_ = nullptr;
    QTabWidget* tabWidget_ = nullptr;

    QWidget* miscTab_ = nullptr;
    ConfirmCloseUnsavedWidget* confirmCloseUnsaved_ = nullptr;

    QWidget* imagesTab_ = nullptr;
    ImageStorageFormatWidget* imageStorageFormat_ = nullptr;
    ArrangeGapWidget* arrangeGap_ = nullptr;
    AllocationLimitWidget* allocationLimit_ = nullptr;
    ArrangeDefaultWidget* arrangeDefault_ = nullptr;

    QWidget* colorsTab_ = nullptr;
    ColorsWidget* colors_ = nullptr;

    QWidget* shortcutsTab_ = nullptr;
    KeyboardShortcutsView* shortcuts_ = nullptr;

    QWidget* mouseTab_ = nullptr;
    MouseView* mouse_ = nullptr;

    QWidget* mouseWheelTab_ = nullptr;
    MouseWheelView* mouseWheel_ = nullptr;
};

#endif // SETTINGSWINDOW_H

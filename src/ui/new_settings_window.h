#ifndef NEWSETTINGSWINDOW_H
#define NEWSETTINGSWINDOW_H

#include <QWidget>

class MainWindow;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class ColorsWidget;
class ConfirmCloseUnsavedWidget;
class ImageStorageFormatWidget;
class ArrangeGapWidget;
class AllocationLimitWidget;
class ArrangeDefaultWidget;
class KeyboardShortcutsView;
class MouseView;
class MouseWheelView;

// PureRef-style prototype of the settings window: sidebar category list +
// stacked pages, instead of SettingsWindow's QTabWidget (ui/settings_window.h).
// Same underlying widgets/logic, different shell - see
// memory/familiar_next_steps.md step 6 for why this exists alongside the
// tabbed version rather than replacing it yet.
class NewSettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit NewSettingsWindow(MainWindow* wm, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    MainWindow* window_ = nullptr;
    QLineEdit* searchBox_ = nullptr;
    QListWidget* categoryList_ = nullptr;
    QStackedWidget* stack_ = nullptr;

    QWidget* miscPage_ = nullptr;
    ConfirmCloseUnsavedWidget* confirmCloseUnsaved_ = nullptr;

    QWidget* imagesPage_ = nullptr;
    ImageStorageFormatWidget* imageStorageFormat_ = nullptr;
    ArrangeGapWidget* arrangeGap_ = nullptr;
    AllocationLimitWidget* allocationLimit_ = nullptr;
    ArrangeDefaultWidget* arrangeDefault_ = nullptr;

    QWidget* colorsPage_ = nullptr;
    ColorsWidget* colors_ = nullptr;

    QWidget* shortcutsPage_ = nullptr;
    KeyboardShortcutsView* shortcuts_ = nullptr;

    QWidget* mousePage_ = nullptr;
    MouseView* mouse_ = nullptr;

    QWidget* mouseWheelPage_ = nullptr;
    MouseWheelView* mouseWheel_ = nullptr;
};

#endif // NEWSETTINGSWINDOW_H

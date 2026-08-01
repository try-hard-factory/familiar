#ifndef NEWSETTINGSWINDOW_H
#define NEWSETTINGSWINDOW_H

#include <QWidget>

class MainWindow;
class QLineEdit;
class QButtonGroup;
class QStackedWidget;
class ColorsWidget;
class ConfirmCloseUnsavedWidget;
class ImageStorageFormatWidget;
class ArrangeGapWidget;
class AllocationLimitWidget;
class ArrangeDefaultWidget;
class KeyboardShortcutsPage;

// Prototype of the settings window: sidebar category list + stacked pages,
// instead of SettingsWindow's QTabWidget (ui/settings_window.h). Same
// underlying widgets/logic, different shell - see
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
    QWidget* categoryPanel_ = nullptr;
    QButtonGroup* categoryButtons_ = nullptr;
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

    KeyboardShortcutsPage* keyboardShortcutsPage_ = nullptr;
};

#endif // NEWSETTINGSWINDOW_H

#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QList>
#include <QWidget>

class MainWindow;
class QLineEdit;
class QButtonGroup;
class QStackedWidget;
class ColorsWidget;
class ConfirmCloseUnsavedWidget;
class AutosaveEnabledWidget;
class AutosaveIntervalWidget;
class ImageStorageFormatWidget;
class ArrangeGapWidget;
class AllocationLimitWidget;
class ArrangeDefaultWidget;
class KeyboardShortcutsPage;
class QPushButton;

// Sidebar category list + stacked pages settings window.
class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(MainWindow* wm, QWidget* parent = nullptr);

    // Jumps straight to the category whose label (tr("Keyboard
    // Shortcuts"), tr("Colors"), ...) matches `name` exactly - e.g.
    // HelpDialog's "Keyboard Shortcuts" link opens this window already
    // on that page instead of whatever was last selected. No-op
    // (silently) if nothing matches.
    void selectCategory(const QString& name);

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    // One nav button + the page it switches to - kept around so the
    // search box can decide per-category whether to show the button
    // (name match) and/or filter the page's own content (content match).
    // `button` is actually a CategoryNavButton (ui/settings_window.cpp)
    // - declared as the plain base here since that class is file-local;
    // its setLabelText() draws the rich text itself, since
    // QPushButton::setText() can't render the <b> a search match needs.
    struct SettingsCategory
    {
        QPushButton* button = nullptr;
        QWidget* page = nullptr;
        QString name;
    };

    MainWindow* window_ = nullptr;
    QLineEdit* searchBox_ = nullptr;
    QWidget* categoryPanel_ = nullptr;
    QButtonGroup* categoryButtons_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QList<SettingsCategory> categories_;

    QWidget* miscPage_ = nullptr;
    ConfirmCloseUnsavedWidget* confirmCloseUnsaved_ = nullptr;
    AutosaveEnabledWidget* autosaveEnabled_ = nullptr;
    AutosaveIntervalWidget* autosaveInterval_ = nullptr;

    QWidget* imagesPage_ = nullptr;
    ImageStorageFormatWidget* imageStorageFormat_ = nullptr;
    ArrangeGapWidget* arrangeGap_ = nullptr;
    AllocationLimitWidget* allocationLimit_ = nullptr;
    ArrangeDefaultWidget* arrangeDefault_ = nullptr;

    QWidget* colorsPage_ = nullptr;
    ColorsWidget* colors_ = nullptr;

    KeyboardShortcutsPage* keyboardShortcutsPage_ = nullptr;
};

#endif // SETTINGSWINDOW_H

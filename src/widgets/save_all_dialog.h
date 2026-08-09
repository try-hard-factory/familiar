#pragma once

#include <map>
#include <QDialog>
#include <QMap>
#include <QString>

class MainWindow;
class QCheckBox;
class QMouseEvent;
class QResizeEvent;
class QVBoxLayout;

// "You have unsaved documents" confirmation shown by
// MainWindow::checkSave() when quitting with modified tabs still open -
// replaces the old plain QWidget-based SaveAllWindow (roadmap step 24,
// Max: "переделаем кастомный виджет который вызывается SaveAllWindow
// ... красиво все элементы переделать"). Same custom-chrome convention
// as CustomMessageBox/dialog_style.h: frameless, opaque, rounded panel
// sourced from the current color preset.
//
// Fire-and-forget like RecoveryDialog (widgets/dialogs.h): constructs,
// shows and self-deletes (WA_DeleteOnClose) on its own - the caller
// doesn't hold onto the pointer or call show() itself.
class SaveAllDialog : public QDialog
{
    Q_OBJECT

public:
    // `items`: tab index -> path (or empty for an untitled tab), one
    // entry per currently-modified tab - same map MainWindow::
    // checkSave() already built for the old SaveAllWindow.
    SaveAllDialog(MainWindow* wm, std::map<int, QString> items, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onCloseWithoutSave_();
    void onSave_();

    MainWindow* window_;
    QMap<int, QCheckBox*> checkboxes_;
};

#pragma once

#include <QDialog>

class MainWindow;
class QMouseEvent;

// "Help" (F1, the "help" action) - two-section layout
// (Controls + Support), adapted from a reference.
// Its own external links (handbook, keyboard shortcuts web page, FAQ,
// forums) are all skipped - no site yet - replaced where a real in-app
// equivalent exists (Settings' Keyboard Shortcuts page, the Debug Log
// dialog) instead of just being cut with
// nothing in their place.
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(MainWindow* wm, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    MainWindow* window_;
};

#pragma once

#include <QDialog>

class MainWindow;
class QMouseEvent;
class QResizeEvent;

// "About" (the "about" action) - layout adapted from a
// reference: app name/version centered, a
// short description, copyright. That reference also has its
// own site/changelog/licenses links - all skipped here,
// same reasoning as HelpDialog (no site yet).
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(MainWindow* wm, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

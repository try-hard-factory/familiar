#pragma once

#include <QDialog>

class MainWindow;
class QMouseEvent;
class QResizeEvent;

// "About" (the "about" action) - PureRef-style layout, adapted from a
// reference screenshot Max provided: app name/version centered, a
// short description, copyright. PureRef's own screenshot also has
// "www.pureref.com"/"Changelog"/"Licenses" links - all skipped here,
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

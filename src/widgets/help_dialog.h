#pragma once

#include <QDialog>

class MainWindow;
class QMouseEvent;

// "Help" (F1, the "help" action) shows controls and a support link.
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

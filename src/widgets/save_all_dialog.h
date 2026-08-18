#pragma once

#include <map>
#include <QDialog>
#include <QMap>
#include <QString>

class MainWindow;
class QCheckBox;
class QMouseEvent;
class QVBoxLayout;

class SaveAllDialog : public QDialog
{
    Q_OBJECT

public:
    // `items`: tab index -> path (or empty for an untitled tab), one
    // entry per currently-modified tab - same map MainWindow::
    // checkSave() already built for the old SaveAllWindow.
    SaveAllDialog(MainWindow* wm,
                  std::map<int, QString> items,
                  QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void onCloseWithoutSave_();
    void onSave_();

    MainWindow* window_;
    QMap<int, QCheckBox*> checkboxes_;
};

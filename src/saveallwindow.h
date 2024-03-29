#ifndef SAVEALLWINDOW_H
#define SAVEALLWINDOW_H
#include <map>
#include <savecheckbox.h>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow;
class SaveAllWindow : public QWidget
{
    Q_OBJECT
public: // move semathics
    SaveAllWindow(MainWindow* wm,
                  std::map<int, QString> items,
                  QWidget* parent = nullptr);
    ~SaveAllWindow();
private slots:
    void onCloseWithoutSaveClicked();
    void onCancelClicked();
    void onSaveClicked();
    void onSaveBoxToggled();

private:
    MainWindow* window_;
    QVBoxLayout* vlayout_;
    QHBoxLayout* hlayout_;
    QLabel* toplabel_;
    QLabel* midlabel_;
    QPushButton* closeWS_btn;
    QPushButton* cancel_btn;
    QPushButton* save_btn;
    std::map<int, SaveCheckBox*> savecheckboxMap_;
};

#endif // SAVEALLWINDOW_H

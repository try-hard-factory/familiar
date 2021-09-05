#ifndef SAVEALLWINDOW_H
#define SAVEALLWINDOW_H
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <savecheckbox.h>
#include <map>

class MainWindow;
class SaveAllWindow : public QWidget
{
    Q_OBJECT
public:                                    // move semathics
    explicit SaveAllWindow(MainWindow* wm, std::map<int, QString> items, QWidget *parent = nullptr);
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

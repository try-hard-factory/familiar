#ifndef SAVEALLWINDOW_H
#define SAVEALLWINDOW_H
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

class MainWindow;
class SaveAllWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SaveAllWindow(MainWindow* wm, QWidget *parent = nullptr);
    ~SaveAllWindow();
private slots:
    void onCloseWithoutSaveClicked();
    void onCancelClicked();
    void onSaveClicked();

private:
    MainWindow* window_;
    QVBoxLayout* vlayout_;
    QHBoxLayout* hlayout_;
    QLabel* label_;
    QPushButton* closeWS_btn;
    QPushButton* cancel_btn;
    QPushButton* save_btn;
};

#endif // SAVEALLWINDOW_H

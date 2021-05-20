#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <canvasview.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QFileDialog;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /*!
     * \brief MainWindow - main window class constructor
     * \param parent
     */
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public slots:
    void onFilterSelected();
private slots:
    void on_action_open_triggered();
    void on_action_saveas_triggered();

private:
    Ui::MainWindow *ui;
    uchar* data_;
    QTimer* mainTimer_;
    CanvasView* canvasWidget;
    QFileDialog* fileDialog_;
    std::unordered_map<QString, QString> fileExt_;
};
#endif // MAINWINDOW_H

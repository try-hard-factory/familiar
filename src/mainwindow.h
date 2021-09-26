#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/**
 *  @file   mainwindow.h
 *  \~russian @brief  Класс главного окна QT
 *  \~russian @author max aka angeleyes (mpano91@gmail.com)
 *  \~russian @bug пока нет:).
 *
 *  \~english @brief  Main window QT class
 *  \~english @author max aka angeleyes (mpano91@gmail.com)
 *  \~english @bug No known bugs.
 */

#include <QMainWindow>
#include <QMessageBox>
#include <canvasview.h>
#include "tabpane.h"
#include "file_actions.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class project_settings;
class QFileDialog;
class SaveAllWindow;
/**
 * \~russian @brief The MainWindow класс
 *
 * \~english @brief The MainWindow class
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    /**
     * \~russian @brief конструктор
     * \~russian @param parent - указатель на QWidget(может быть nullptr - это
     *                           нормально)
     *
     * \~english @brief main window class constructor
     * \~english @param parent - pointer to QWidget parent(may be nullptr - it
     *                           is normal)
     */
    MainWindow(QWidget *parent = nullptr);

    /**
     * \~russian @brief деструктор
     *
     * \~english @brief class destructor
     */
    ~MainWindow();


    /* Part of feature FileActions class */
    void newFile();
    void openFile();
    int saveFile();
    int saveFileAs();
    void settingsWindow();
    void quitProject();

    void saveAllWindowSaveCB(SaveAllWindow* w, std::map<int, bool>&& m);
    void cleanupWorkplace();
    void exitProject();
    TabPane& tabPane();
    FileActions& fileActions();

    void clipboardItems(QVector<QGraphicsItem*> ci) noexcept { clipboardItems_ = ci; }
    QVector<QGraphicsItem*>& clipboardItems() noexcept { return clipboardItems_; }
    void clearClipboardItems() { clipboardItems_.clear();}

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void saveFile(const QString& path);
    bool checkSave();

private slots:
    void on_action_save_all_triggered();
    void on_action_new_triggered();
    void on_action_settings_triggered();
    void on_action_save_triggered();
    void on_action_quit_triggered();
    /**
     * \~russian @brief QT обработчик для кнопки открытия файла
     *
     * \~english @brief QT slot-handler for "open file" button from main menu
     */
    void on_action_open_triggered();

    /**
     * \~russian @brief QT обработчик для кнопки "сохранить как"
     *
     * \~english @brief QT slot-handler for "save as" button from main menu
     */
    void on_action_saveas_triggered();

private:
    Ui::MainWindow *ui; ///< \~english Main window ui \~russian графический интерфейс главного окна
    FileActions* fileactions_;
    TabPane* tabpane_;
    QVector<QGraphicsItem*> clipboardItems_;
};

template<typename M, typename S>
void centered_widget(M* mw, S* w) {
    auto hostRect = mw->geometry();
    auto point = hostRect.center() - QPoint(w->size().width()/2,
                                            w->size().height()/2);
    w->move(point);
}
#endif // MAINWINDOW_H

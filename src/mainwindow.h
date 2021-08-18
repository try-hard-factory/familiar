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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class project_settings;
class QFileDialog;
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
    void openFile();
    int saveFile();
    int saveFileAs();
    void quitProject();

    void cleanupWorkplace();


protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void saveFile(const QString& path);

private slots:
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
    CanvasView* canvasWidget; ///< \~english app working place \~russian рабочее пространство приложения
    QFileDialog* fileDialog_; ///< \~english file dialog operations \~russian диалоговое окно для операций с файлами
    std::unordered_map<QString, QString> fileExt_; ///< \~english table with file extention \~russian таблица с расширениями файлов
    project_settings* projectSettings_;
};
#endif // MAINWINDOW_H

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

#include "file_actions.h"
#include "tabpane.h"
#include <canvasview.h>
#include <QMainWindow>
#include <QMessageBox>
#include <QCursor>
#include <core/settingshandler.h>
#include <utils/utils.h>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class project_settings;
class QFileDialog;
class SaveAllWindow;
class QShortcut;

enum EShortcutButtons {
    k_TYPE_NEW = 0,
    k_TYPE_OPEN = 1,
    k_TYPE_SAVE = 2,
    k_TYPE_QUIT = 3,
    k_ALL = 4
};

constexpr QPoint kInvalidPoint(-1, -1);

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
    MainWindow(QWidget* parent = nullptr);

    /**
     * \~russian @brief деструктор
     *
     * \~english @brief class destructor
     */
    ~MainWindow();


    /* Part of feature FileActions class */
    void quitProject();

    void saveAllWindowSaveCB(SaveAllWindow* w, std::map<int, bool>&& m);
    void cleanupWorkplace();
    void exitProject();
    TabPane& tabPane();
    FileActions& fileActions();

    void clipboardItems(QVector<QGraphicsItem*> ci) noexcept
    {
        clipboardItems_ = ci;
    }
    QVector<QGraphicsItem*>& clipboardItems() noexcept
    {
        return clipboardItems_;
    }
    void clearClipboardItems() { clipboardItems_.clear(); }

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    // void mousePressEvent(QMouseEvent* _event)
    // {
    //     if (_event->button() == Qt::LeftButton
    //         && _event->modifiers() == Qt::NoModifier) {
    //         pos_ = _event->globalPosition().toPoint();
    //         setCursor(Qt::ClosedHandCursor);
    //         return;
    //     }
    //     QWidget::mousePressEvent(_event);
    // }
    // void mouseMoveEvent(QMouseEvent* _event)
    // {
    //     if (pos_ == kInvalidPoint)
    //         return QWidget::mouseMoveEvent(_event);

    //     const QPoint delta = _event->globalPosition().toPoint() - pos_;
    //     move(pos() + delta);
    //     pos_ = _event->globalPosition().toPoint();
    // }

    // void mouseReleaseEvent(QMouseEvent* _event)
    // {
    //     pos_ = kInvalidPoint;
    //     setCursor(Qt::OpenHandCursor);
    //     QWidget::mouseReleaseEvent(_event);
    // }

private:
    QPoint pos_ = kInvalidPoint;

private:
    //    void saveFile(const QString& path);
    bool checkSave();
    void initShortcuts();
    void newShortcut(EShortcutButtons as_key,
                     const QKeySequence& key,
                     QWidget* parent,
                     const char* slot);
    void createActions();
    void createMenus();

public slots:
    void settingsChangedSlot();
private slots:
    void saveAll();
    void newFile();
    void settingsWindow();
    void saveFile();
    void quit();
    void openFile();
    void saveFileAs();
    void notifyShortcut(const QString& t);


private:
    SettingsHandler settings_;
    Ui::MainWindow*
        ui; ///< \~english Main window ui \~russian графический интерфейс главного окна
    FileActions* fileactions_ = nullptr;
    TabPane* tabpane_ = nullptr;
    QVector<QGraphicsItem*> clipboardItems_;

    QMenu* fileMenu_ = nullptr;
    QAction* saveAllAction_ = nullptr;
    QAction* newAction_ = nullptr;
    QAction* settingsAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* saveAsAction_ = nullptr;

    int currentOpacity_;
    QColor backGroundColor_;
    QString rgbaBackGroundStr_;

    QVarLengthArray<QAction*, EShortcutButtons::k_ALL> actionsArr_
        = {nullptr, nullptr, nullptr, nullptr};
    //    QVarLengthArray<QShortcut*, EShortcutButtons::k_ALL> shortcutArr_;
};

#endif // MAINWINDOW_H

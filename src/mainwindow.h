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
#include <core/settingshandler.h>
#include <utils/utils.h>
#include <QApplication>
#include <QCursor>
#include <QMainWindow>
#include <QMessageBox>
#include <QWindow>
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
    ~MainWindow();

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

protected:
    void mouseMoveEvent(QMouseEvent* event) override
    {
        updateResizeCursor(event->pos());
        QMainWindow::mouseMoveEvent(event);
    }

    // Central widget (and children like menuBar()) cover the whole frameless
    // window, so QMainWindow rarely gets its own mouse events while hovering
    // or clicking over them. Watch every mouse move/press application-wide so
    // the resize cursor and the drag-to-resize border work regardless of
    // which child widget actually received the event.
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            updateResizeCursor(
                mapFromGlobal(mouseEvent->globalPosition().toPoint()));
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                tryStartSystemResize(
                    mapFromGlobal(mouseEvent->globalPosition().toPoint()));
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

private:
    static constexpr int kResizeBorder
        = 10; // Толщина невидимой границы для ресайза, в пикселях

    Qt::Edges resizeEdgesAt(const QPoint& pos) const
    {
        Qt::Edges edges;
        if (pos.x() < kResizeBorder)
            edges |= Qt::LeftEdge;
        if (pos.x() > width() - kResizeBorder)
            edges |= Qt::RightEdge;
        if (pos.y() < kResizeBorder)
            edges |= Qt::TopEdge;
        if (pos.y() > height() - kResizeBorder)
            edges |= Qt::BottomEdge;
        return edges;
    }

    void tryStartSystemResize(const QPoint& pos)
    {
        if (!rect().contains(pos) || !windowHandle())
            return;

        // Запуск нативного изменения размера (доступно в Qt 5.15 и новее)
        const Qt::Edges edges = resizeEdgesAt(pos);
        if (edges) {
            windowHandle()->startSystemResize(edges);
        }
    }

    void updateResizeCursor(const QPoint& pos)
    {
        if (!rect().contains(pos)) {
            unsetCursor();
            return;
        }

        const Qt::Edges edges = resizeEdgesAt(pos);

        if ((edges & Qt::LeftEdge && edges & Qt::TopEdge)
            || (edges & Qt::RightEdge && edges & Qt::BottomEdge))
            setCursor(Qt::SizeFDiagCursor);
        else if ((edges & Qt::RightEdge && edges & Qt::TopEdge)
                 || (edges & Qt::LeftEdge && edges & Qt::BottomEdge))
            setCursor(Qt::SizeBDiagCursor);
        else if (edges & (Qt::LeftEdge | Qt::RightEdge))
            setCursor(Qt::SizeHorCursor);
        else if (edges & (Qt::TopEdge | Qt::BottomEdge))
            setCursor(Qt::SizeVerCursor);
        else
            unsetCursor();
    }

private:
    QPoint pos_ = kInvalidPoint;

private:
    bool checkSave();

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
    Ui::MainWindow* ui;
    FileActions* fileactions_ = nullptr;
    TabPane* tabpane_ = nullptr;
    QVector<QGraphicsItem*> clipboardItems_;

    int currentOpacity_;
    QColor backGroundColor_;
    QString rgbaBackGroundStr_;
};

#endif // MAINWINDOW_H

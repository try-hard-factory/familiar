#ifndef CANVASVIEW_H
#define CANVASVIEW_H

/**
 *  @file   canvasview.h
 *  \~russian @brief  Графическоe представлениe рабочей области
 *  \~russian @author max aka angeleyes (mpano91@gmail.com)
 *
 *  \~english @brief  Graphics view of working place
 *  \~english @author max aka angeleyes (mpano91@gmail.com)
 */

#include <QObject>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QScrollBar>

class MainWindow;
class project_settings;
class CanvasScene;

/**
 * \~russian @brief CanvasView класс
 *
 * \~english @brief The CanvasView class
 */
class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    /**
     * \~russian @brief конструктор
     * \~russian @param parent - указатель на QWidget(может быть nullptr - это
     *                           нормально)
     * \~russian @param ps - настройки проекта
     *
     * \~english @brief main window class constructor
     * \~english @param parent - pointer to QWidget parent(may be nullptr - it
     *                           is normal)
     * \~english @param ps - project settings
     */
    CanvasView(MainWindow& mw, QWidget* parent = 0);

    /**
     * \~russian @brief деструктор
     *
     * \~english @brief class destructor
     */
    ~CanvasView();

    /**
     * \~russian @brief setProjectSettings
     * \~russian @param ps - настройки проекта
     *
     * \~english @brief setProjectSettings
     * \~english @param ps - project settings
     */
    void setProjectSettings(project_settings* ps);

    /**
     * \~russian @brief добавить изображение на сцену
     * \~russian @param path - путь к файлу
     * \~russian @param point - координаты точки на сцене
     *
     * \~english @brief addImage add Image to the scene
     * \~english @param path - path of image
     * \~english @param point - coords of image on scene
     */
    void addImage(const QString& path, QPointF point);

    /**
     * \~russian @brief добавить изображение на сцену
     * \~russian @param img - изображение
     * \~russian @param point - координаты точки на сцене
     *
     * \~english @brief add Image to the scene
     * \~english @param img - image
     * \~english @param point - coords of image on scene
     */
    void addImage(QImage* img, QPointF point);

    void addImage(QByteArray ba, int w, int h, QRect br, qsizetype bpl, QImage::Format f);

    void SetScale(qreal qrScale);
    void ScaleView(qreal qFactor);

    void zoomFactor(double factor) noexcept { zoomFactor_ = factor; }
    double zoomFactor() const noexcept { return zoomFactor_; }

    QByteArray fml_payload();

    void cleanupWorkplace();

    QString path();
    void setPath(const QString& path);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();
public slots:
    /**
     * \~russian @brief QT обработчик для события выбора элементов
     *
     * \~english @brief QT slot-handler for selection changed
     */
    void onSelectionChanged();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    /**
     * \~russian @brief перегрузите эту функцию, для обработки движений мышки
     * \~russian @param event - событие движения мышки
     *
     * \~english @brief overload this function to process mouse moves
     * \~english @param event - mouse move event
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки нажатий клавиш мышки
     * \~russian @param event - событие нажатий клавиш мышки
     *
     * \~english @brief overload this function to process mouse button pressed
     * \~english @param event - mouse press event
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки отпускания клавиш мышки
     * \~russian @param event - событие отпускания клавиш мышки
     *
     * \~english @brief overload this function to process mouse button released
     * \~english @param event - mouse release event
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки колёсика мышки
     * \~russian @param event - событие колёсика мышки
     *
     * \~english @brief overload this function to process mouse wheel
     * \~english @param event - mouse wheel event
     */
    void wheelEvent(QWheelEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки изменения размера
     * \~russian @param event - событие колёсика мышки
     *
     * \~english @brief overload this function to process resize event
     * \~english @param event - resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для рисования на сцене
     * \~russian @param painter - используется для рисования на сцене
     * \~russian @param rect - прямоугольник для отрисовки
     *
     * \~english @brief overload this function to Draws the background of the scene
     * \~english @param painter - using for painting on scene
     * \~english @param rect - is the exposed rectangle.
     */
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    MainWindow& mainwindow_;
    CanvasScene* scene_;
    double zoomFactor_ = 1.15;
    Qt::MouseButton pan_;
    bool rightMoveflag_{false};
    QPoint pressPos_;
    QPoint wndPos_;
    bool isMoving_{false};;
    qreal panStartX_;
    qreal panStartY_;
    uint64_t zCounter_ = 0;
};

#endif // CANVASVIEW_H

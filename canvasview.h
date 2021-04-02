#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QObject>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QScrollBar>

class CanvasScene;
#define ZOOM_FACTOR 1.15

class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    CanvasView(QWidget* parent = 0);
    ~CanvasView();

    void addImage(const QString& path, QPointF point);
    void setZoomFactor(double factor) { zoomFactor_ = factor; }

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent *event) override;
    virtual QString setToolTipText(QPoint imageCoordinates);
    void drawBackground(QPainter *painter, const QRectF &rect) override;
private:
    CanvasScene* scene_;
    double zoomFactor_;
    bool pan_;
    bool panStartX_;
    bool panStartY_;
    uint64_t zCounter_ = 0;
public slots:
    void onSelectionChanged();
};

#endif // CANVASVIEW_H

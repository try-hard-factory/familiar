#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
class CanvasScene : public QGraphicsScene
{
public:
    CanvasScene(QGraphicsScene *scene = 0);
    ~CanvasScene() = default;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

private:
    bool isAnySelectedUnderCursor() const;
};

#endif // CANVASSCENE_H

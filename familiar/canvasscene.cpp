#include "canvasscene.h"
#include <QKeyEvent>
#include <QGraphicsItem>
//#define MOUSE_MOVE_DEBUG
CanvasScene::CanvasScene(QGraphicsScene *scene)
{

}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        qDebug() << "Selected items "<< selectedItems().size();
        for (auto it : selectedItems()) {
            removeItem(it);
        }
//        while (!selectedItems().isEmpty()) {
//            removeItem(selectedItems().front());
//        }
    } else {
        QGraphicsScene::keyPressEvent(event);
    }
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo()<<"CanvasScene::mousePressEvent. "
          <<", Event->scenePos: "<<event->scenePos();


    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() != Qt::ShiftModifier) {
            if (!isAnySelectedUnderCursor()) clearSelection();
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    qInfo()<<"CanvasScene::mouseReleaseEvent. "
          <<", Event->scenePos: "<<event->scenePos();
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() != Qt::ShiftModifier) {
            if (isAnySelectedUnderCursor()) clearSelection();
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#ifdef MOUSE_MOVE_DEBUG
    qInfo()<<"CanvasScene::mousePressEvent. "
          <<", Event->scenePos: "<<event->scenePos();
#endif
    QGraphicsScene::mouseReleaseEvent(event);
}

bool CanvasScene::isAnySelectedUnderCursor() const
{
    auto selItems = selectedItems();
    for (auto& it : selItems) {
        if (it->isUnderMouse()) return true;
    }

    return false;
}


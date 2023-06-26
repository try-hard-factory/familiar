#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include "image_downloader.h"
#include "mainselectedarea.h"
#include <itemgroup.h>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QRubberBand>

enum EState {
    eMouseMoving = 0x0000,
    eMouseSelection = 0x0001,
    eGroupItemMoving = 0x0002,
    eGroupItemResizing = 0x0003,
};

class MainWindow;
class project_settings;

class CanvasScene : public QGraphicsScene
{
public:
    CanvasScene(MainWindow& mw, uint64_t& zc, QGraphicsScene* scene = 0);
    ~CanvasScene();

public:
    void pasteFromClipboard();
    void pasteFromTemp();
    void copyToClipboard();
    void onSelectionChanged();
    QGraphicsItem* getFirstItemUnderCursor(const QPointF& p);
    void addImageToSceneToPosition(QImage&& image, QPointF position);
    QByteArray fml_payload();
    void updateViewScaleFactor(qreal newval)
    {
        itemGroup_->setScaleControlFactor(newval);
        parentViewScaleFactor_ = newval;
    }

    void setProjectSettings(project_settings* ps);

    void cleanupWorkplace();

    QString path();
    void setPath(const QString& path);
    QString projectName();
    void setProjectName(const QString& pn);
    bool isModified();
    void setModified(bool mod);
    bool isUntitled();
    ItemGroup* itemGroup() const noexcept { return itemGroup_; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;

public slots:
    void slotMove(QGraphicsItem* signalOwner, qreal dx, qreal dy);
    void settingsChangedSlot();
private slots:
    void clipboardChanged();

private:
    qint16 objectsCount() const;
    bool isAnySelectedUnderCursor() const;
    void deselectItems();

    void handleImageFromClipboard(const QImage& image);
    void handleHtmlFromClipboard(const QString& html);

    MainWindow& mainwindow_;
    uint64_t& zCounter_;
    ItemGroup* itemGroup_ = nullptr;
    MainSelectedArea mainSelArea_;

    EState state_ = eMouseMoving;

    ImageDownloader* imgdownloader_ = nullptr; //image loader need c++threads!!!

    qreal parentViewScaleFactor_ = 1;
    project_settings* projectSettings_;

    QPointF origin_;
    QRectF rubberBand_;
    QPointF lastClickedPoint_{0, 0};
    QColor selectionColor_;
};

#endif // CANVASSCENE_H

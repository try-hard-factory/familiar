#ifndef BORDERDOT_H
#define BORDERDOT_H

#include <QObject>
#include <QGraphicsRectItem>

class QGraphicsSceneHoverEventPrivate;
class QGraphicsSceneMouseEvent;

class DotSignal : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF previousPosition READ previousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)

public:
    explicit DotSignal(QGraphicsItem *parentItem = 0, QObject *parent = 0);
    explicit DotSignal(QPointF pos, QGraphicsItem *parentItem = 0, QObject *parent = 0);
    ~DotSignal();

    enum Flags {
        Movable = 0x01
    };

    enum { Type = UserType + 1 };

    int type() const override
    {
        return Type;
    }
    QPointF previousPosition() const;
    void setPreviousPosition(const QPointF previousPosition);

    void setDotFlags(unsigned int flags);

signals:
    void previousPositionChanged();
    void signalMouseRelease();
    void signalMove(QGraphicsItem *signalOwner, qreal dx, qreal dy);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

public slots:

private:
    unsigned int m_flags;
    QPointF m_previousPosition;
};

#endif // BORDERDOT_H

#ifndef BORDERDOT_H
#define BORDERDOT_H

#include <QGraphicsRectItem>
#include <QObject>

class QGraphicsSceneHoverEventPrivate;
class QGraphicsSceneMouseEvent;

/**
 * \~russian @brief DotSignal класс
 *
 * \~english @brief The DotSignal class
 */
class DotSignal : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF previousPosition READ previousPosition WRITE
                   setPreviousPosition NOTIFY previousPositionChanged)

public:
    explicit DotSignal(QGraphicsItem* parentItem = nullptr, QObject* parent = nullptr);
    // explicit DotSignal(QPointF pos,
    //                    QGraphicsItem* parentItem = 0,
    //                    QObject* parent = 0);
    ~DotSignal();

    enum Flags { Movable = 0x01 };

    enum { Type = UserType + 1 };

    int type() const override { return Type; }
    QPointF previousPosition() const noexcept;
    void setPreviousPosition(const QPointF previousPosition) noexcept;

    void setDotFlags(unsigned int flags);
    void SetScale(qreal qrScale);

public slots:
    void settingsChangedSlot();
    
signals:
    void previousPositionChanged();
    void signalMouseRelease();

    /**
     * @brief signalMove
     * @param signalOwner
     * @param dx
     * @param dy
     */
    void signalMove(QGraphicsItem* signalOwner, qreal dx, qreal dy);

protected:
    /**
     * \~russian @brief перегрузите эту функцию, для обработки движений мышки
     * \~russian @param event - событие движения мышки
     *
     * \~english @brief overload this function to process mouse moves
     * \~english @param event - mouse move event
     */
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки нажатий клавиш мышки
     * \~russian @param event - событие нажатий клавиш мышки
     *
     * \~english @brief overload this function to process mouse button pressed
     * \~english @param event - mouse press event
     */
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки отпускания клавиш мышки
     * \~russian @param event - событие отпускания клавиш мышки
     *
     * \~english @brief overload this function to process mouse button released
     * \~english @param event - mouse release event
     */
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки события наведения на элемент
     * \~russian @param event - событие наведения на элемент
     *
     * \~english @brief overload this function to process hover enter event
     * \~english @param event - hover event
     */
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;

    /**
     * \~russian @brief перегрузите эту функцию, для обработки события наведения на элемент
     * \~russian @param event - событие наведения на элемент
     *
     * \~english @brief overload this function to process hover leave event
     * \~english @param event - hover event
     */
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

public slots:

private:
    unsigned int m_flags;
    QPointF m_previousPosition;
    QColor selectionColor_;
};

#endif // BORDERDOT_H

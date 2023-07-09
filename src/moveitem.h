#ifndef MOVEITEM_H
#define MOVEITEM_H

#include <QCursor>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QObject>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QWheelEvent>
// QPlainTextEdit *plainTextEdit = new QPlainTextEdit();
// QGraphicsProxyWidget *proxyWidget = new QGraphicsProxyWidget();
// proxyWidget->setWidget(plainTextEdit);

// QGraphicsItemGroup *itemGroup = new QGraphicsItemGroup();
// itemGroup->addToGroup(proxyWidget);
// scene->addItem(itemGroup);


class MoveItemBase : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MoveItemBase(QObject* parentobj = nullptr, QGraphicsItem* parent = nullptr)
        : QObject(parentobj)
        , QGraphicsItem(parent)
    {}
    virtual ~MoveItemBase() {}
    enum { Type = UserType + 10 };

    int type() const override
    {
        qDebug() << "MoveItemBase::type: " << Type;
        return Type;
    }

protected:
    // void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    // {
    //     qDebug() << "mouseDoubleClickEvent "
    //                 "MoveItemBase!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    // }
};

class TextItem : public MoveItemBase
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    explicit TextItem()
        : MoveItemBase()
    {
        setFlag(QGraphicsItem::ItemIsSelectable);
        setFlag(QGraphicsItem::ItemIsMovable);
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton);
        textItem = new QGraphicsTextItem(this);
        textItem->setPlainText("note");
        textItem->setPos(boundingRect().topLeft());
    }

    QRectF boundingRect() const override { return textItem->boundingRect(); }

    void setText(const QString& text)
    {
        textItem->setPlainText("note");
        update();
    }


    enum { Type = UserType + 2 };

    int type() const override { return Type; }

    void startTextEdit()
    {
        if (textItem->textInteractionFlags() == Qt::NoTextInteraction) {
            textItem->setTextInteractionFlags(Qt::TextEditable);
            setFocus();
        }

        // ensure that when enabling the edit mode, the cursor is in an appropriate position (close to where the double-click happened)
        auto cursor = textItem->textCursor();
        cursor.movePosition(QTextCursor::End);
        textItem->setTextCursor(cursor);
        textItem->update();
        qDebug() << "$$$$$$$TextItem::startTextEdit";
    }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        // painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        qDebug() << "TextItem::mouseDoubleClickEvent "
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        startTextEdit();
        QGraphicsItem::mouseDoubleClickEvent(event);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        qDebug() << "TextItem::mousePressEvent "
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        QGraphicsItem::mousePressEvent(event);
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
        qDebug() << "TextItem::hoverEnterEvent "
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        QGraphicsItem::hoverEnterEvent(event);
    }

    bool sceneEvent(QEvent* event) override
    {
        qDebug() << "TextItem::sceneEvent "
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                 << event->type();
        if (event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
            QGraphicsSceneMouseEvent* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                startTextEdit();
                return true; // Поглощаем событие двойного щелчка мыши
            }
        }
        return QGraphicsItem::sceneEvent(event);
    }


private:
    QGraphicsTextItem* textItem = nullptr;
};

class MoveItem : public MoveItemBase
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MoveItem(const QString& path, uint64_t& zc, MoveItemBase* parent = nullptr);
    MoveItem(QImage* img, uint64_t& zc, MoveItemBase* parent = nullptr);
    MoveItem(QByteArray ba,
             int w,
             int h,
             qsizetype bpl,
             QImage::Format f,
             uint64_t& zc,
             MoveItemBase* parent = nullptr);
    ~MoveItem();

    enum { Type = UserType + 3 };
    int type() const override { return Type; }

    QRectF getRect() const { return boundingRect(); }
    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF& rect);
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    int height() const noexcept { return qimage_->height(); }
    int width() const noexcept { return qimage_->width(); }
    QImage::Format format() const noexcept { return qimage_->format(); }
    const QImage& qimage() { return *qimage_; }
    const QImage* qimage_ptr_const() { return qimage_; }
    QImage* qimage_ptr() { return qimage_; }
    uint64_t& zcounter() noexcept { return zCounter_; }

public slots:
    void settingsChangedSlot();

signals:
protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        qDebug() << "MoveItem::mouseDoubleClickEvent "
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        QGraphicsItem::mouseDoubleClickEvent(event);
    }
    //    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QPointF shiftMouseCoords_;
    QImage* qimage_;
    QPixmap pixmap_;
    QByteArray ba_;
    uint64_t& zCounter_;
    QSizeF _size;
    QRectF rect_;
    QColor selectionColor_;
    int currentOpacity_;
};

#endif // MOVEITEM_H

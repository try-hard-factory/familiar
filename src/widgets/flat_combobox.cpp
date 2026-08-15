#include "flat_combobox.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionComboBox>

FlatComboBox::FlatComboBox(const QColor& background,
                           const QColor& text,
                           const QColor& hoverBackground,
                           QWidget* parent)
    : QComboBox(parent)
    , background_(background)
    , text_(text)
    , hoverBackground_(hoverBackground)
{}

void FlatComboBox::enterEvent(QEnterEvent* event)
{
    QComboBox::enterEvent(event);
    update();
}

void FlatComboBox::leaveEvent(QEvent* event)
{
    QComboBox::leaveEvent(event);
    update();
}

void FlatComboBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool enabled = isEnabled();
    const QColor bg = (enabled && underMouse()) ? hoverBackground_ : background_;
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), 6, 6);

    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    const QRect arrowRect = style()->subControlRect(QStyle::CC_ComboBox,
                                                     &opt,
                                                     QStyle::SC_ComboBoxArrow,
                                                     this);
    const QRect editRect = style()->subControlRect(QStyle::CC_ComboBox,
                                                    &opt,
                                                    QStyle::SC_ComboBoxEditField,
                                                    this);

    QColor fg = text_;
    if (!enabled)
        fg.setAlpha(110);

    p.setPen(fg);
    const QRect textRect = editRect.adjusted(4, 0, -4, 0);
    p.drawText(textRect,
              Qt::AlignVCenter | Qt::AlignLeft,
              fontMetrics().elidedText(currentText(),
                                       Qt::ElideRight,
                                       textRect.width()));

    const qreal cx = arrowRect.center().x();
    const qreal cy = arrowRect.center().y();
    constexpr qreal halfW = 4.0;
    constexpr qreal halfH = 2.5;
    QPainterPath arrow;
    arrow.moveTo(cx - halfW, cy - halfH);
    arrow.lineTo(cx + halfW, cy - halfH);
    arrow.lineTo(cx, cy + halfH);
    arrow.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(fg);
    p.drawPath(arrow);
}

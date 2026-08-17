#include "flat_spinbox.h"

#include <QEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionSpinBox>

FlatSpinBox::FlatSpinBox(const QColor& background,
                         const QColor& text,
                         const QColor& hoverBackground,
                         QWidget* parent)
    : QSpinBox(parent)
    , background_(background)
    , text_(text)
    , hoverBackground_(hoverBackground)
{
    setFrame(false);
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setMinimumHeight(26);

    QColor disabledText = text_;
    disabledText.setAlpha(110);
    lineEdit()->setStyleSheet(
        QStringLiteral("QLineEdit {"
                       "  background: transparent;"
                       "  border: none;"
                       "  color: %1;"
                       "  selection-background-color: transparent;"
                       "  selection-color: %1;"
                       "}"
                       "QLineEdit:disabled { color: %2; }")
            .arg(text_.name(), disabledText.name(QColor::HexArgb)));
}

void FlatSpinBox::resizeEvent(QResizeEvent* event)
{
    QSpinBox::resizeEvent(event);
    QStyleOptionSpinBox opt;
    initStyleOption(&opt);
    const QRect editRect = style()->subControlRect(QStyle::CC_SpinBox,
                                                   &opt,
                                                   QStyle::SC_SpinBoxEditField,
                                                   this);
    lineEdit()->setGeometry(editRect.left(), 0, editRect.width(), height());
}

void FlatSpinBox::enterEvent(QEnterEvent* event)
{
    QSpinBox::enterEvent(event);
    update();
}

void FlatSpinBox::leaveEvent(QEvent* event)
{
    QSpinBox::leaveEvent(event);
    update();
}

void FlatSpinBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool enabled = isEnabled();
    const QColor bg = (enabled && (underMouse() || hasFocus()))
                          ? hoverBackground_
                          : background_;
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), 6, 6);

    QStyleOptionSpinBox opt;
    initStyleOption(&opt);
    const QRect upRect = style()->subControlRect(QStyle::CC_SpinBox,
                                                 &opt,
                                                 QStyle::SC_SpinBoxUp,
                                                 this);
    const QRect downRect = style()->subControlRect(QStyle::CC_SpinBox,
                                                   &opt,
                                                   QStyle::SC_SpinBoxDown,
                                                   this);

    // Text itself is the real QLineEdit's own native rendering (see the
    // header's comment for why NOT hand-drawn here) - only the arrows
    // are custom-painted.
    QColor fg = text_;
    if (!enabled)
        fg.setAlpha(110);
    p.setPen(Qt::NoPen);
    p.setBrush(fg);

    auto drawArrow = [&p](const QRect& r, bool up) {
        const qreal cx = r.center().x();
        const qreal cy = r.center().y();
        constexpr qreal halfW = 3.0;
        constexpr qreal halfH = 2.0;
        QPainterPath path;
        if (up) {
            path.moveTo(cx - halfW, cy + halfH);
            path.lineTo(cx + halfW, cy + halfH);
            path.lineTo(cx, cy - halfH);
        } else {
            path.moveTo(cx - halfW, cy - halfH);
            path.lineTo(cx + halfW, cy - halfH);
            path.lineTo(cx, cy + halfH);
        }
        path.closeSubpath();
        p.drawPath(path);
    };
    drawArrow(upRect, true);
    drawArrow(downRect, false);
}

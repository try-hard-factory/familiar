#include "dialog_style.h"

#include <QColor>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHashFunctions>
#include <QLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegion>
#include <QWidget>
#include <Qt>
#include <QtTypes>

namespace familiar::dialog_style {

namespace {

// Minimum hit area for the dialog buttons, so a one-word label like "OK"
// does not shrink to a target that is awkward to click.
constexpr int kbuttonMinWidth = 76;
constexpr int kbuttonMinHeight = 30;
// How far the accent colour is pushed for the hover and pressed states.
constexpr int kaccentShiftPercent = 115;
// Alpha of the close button's hover tint.
constexpr int kcloseHoverAlpha = 90;

// The severity glyph is laid out as fractions of kiconSize so it scales
// with the icon rather than being pinned to one pixel size: a triangle
// for Warning, a circle for the rest, with the letter inside.
constexpr qreal ktriApexX = 0.5;
constexpr qreal ktriApexY = 0.05;
constexpr qreal ktriRightX = 0.96;
constexpr qreal ktriBaseY = 0.92;
constexpr qreal ktriLeftX = 0.04;
constexpr qreal kcircleInset = 0.05;
constexpr qreal kcircleSize = 0.9;
// The "!" inside the triangle needs to be smaller than the "i"/"?" inside
// the circle, which has more room.
constexpr qreal kglyphScaleTriangle = 0.38;
constexpr qreal kglyphScaleCircle = 0.48;

} // namespace

QColor severity_color(QMessageBox::Icon icon, const QColor& accent)
{
    // Amber / red / blue, as data - the literals are RGB channels of three
    // chosen colours, same call as the palette tables in
    // core/settingshandler.cpp.
    // NOLINTBEGIN(readability-magic-numbers)
    switch (icon) {
    case QMessageBox::Warning:
        return {0xf5, 0xa6, 0x23};
    case QMessageBox::Critical:
        return {0xe5, 0x48, 0x4d};
    case QMessageBox::Information:
        return {0x4a, 0x90, 0xd9};
    case QMessageBox::Question:
    default:
        return accent;
    }
    // NOLINTEND(readability-magic-numbers)
}

namespace {

constexpr int kiconSize = 40;

} // namespace

QString panel_style_sheet(const char* className,
                          const QColor& background,
                          const QColor& border,
                          const QColor& text,
                          int radiusPx)
{
    // The QToolTip rule is here for the same reason every other
    // tooltip-capable control in this app needs one explicitly
    // (MainWindow::updateWindowControlsStyle_(), the tab close button):
    // the window-wide "background: transparent" cascade strips native
    // tooltip rendering, painting it as a solid black plate otherwise.
    // Tooltips have no alpha channel - keep the color fully opaque.
    return QStringLiteral("%1 {"
                          "  background-color: %2;"
                          "  border: 1px solid %3;"
                          "  border-radius: %5px;"
                          "}"
                          "QLabel { background: transparent; color: %4; }"
                          "QToolTip {"
                          "  background-color: %2;"
                          "  color: %4;"
                          "  border: 1px solid %3;"
                          "}")
        .arg(QString::fromUtf8(className),
             background.name(),
             border.name(),
             text.name())
        .arg(radiusPx);
}

void style_primary_button(QPushButton* button, const QColor& accent)
{
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumWidth(kbuttonMinWidth);
    button->setMinimumHeight(kbuttonMinHeight);
    button->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background-color: %1;"
                       "  color: white;"
                       "  border: none;"
                       "  border-radius: 6px;"
                       "  padding: 4px 14px;"
                       "  font-weight: 600;"
                       "}"
                       "QPushButton:hover { background-color: %2; }"
                       "QPushButton:pressed { background-color: %3; }")
            .arg(accent.name(),
                 accent.lighter(kaccentShiftPercent).name(),
                 accent.darker(kaccentShiftPercent).name()));
}

void style_secondary_button(QPushButton* button,
                            const QColor& text,
                            const QColor& border)
{
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumWidth(kbuttonMinWidth);
    button->setMinimumHeight(kbuttonMinHeight);
    button->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: transparent;"
            "  color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 6px;"
            "  padding: 4px 14px;"
            "}"
            "QPushButton:hover { background-color: rgba(255,255,255,18); }"
            "QPushButton:pressed { background-color: rgba(255,255,255,32); }")
            .arg(text.name(), border.name()));
}

QString close_button_style_sheet(const char* objectName,
                                 const QColor& text,
                                 const QColor& accent)
{
    QColor hover = accent;
    hover.setAlpha(kcloseHoverAlpha);
    return QStringLiteral(
               "#%1 {"
               "  background: transparent;"
               "  color: %2;"
               "  border: none;"
               "  border-radius: 11px;"
               "  font-size: 14px;"
               "}"
               "#%1:hover { background-color: rgba(%3, %4, %5, %6); }")
        .arg(QString::fromUtf8(objectName),
             text.name(),
             QString::number(hover.red()),
             QString::number(hover.green()),
             QString::number(hover.blue()),
             QString::number(hover.alpha()));
}

QPixmap severity_icon(QMessageBox::Icon icon, const QColor& accent, qreal dpr)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QColor bg = severity_color(icon, accent);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    if (icon == QMessageBox::Warning) {
        QPainterPath tri;
        tri.moveTo(kiconSize * ktriApexX, kiconSize * ktriApexY);
        tri.lineTo(kiconSize * ktriRightX, kiconSize * ktriBaseY);
        tri.lineTo(kiconSize * ktriLeftX, kiconSize * ktriBaseY);
        tri.closeSubpath();
        p.drawPath(tri);
    } else {
        p.drawEllipse(QRectF(kiconSize * kcircleInset,
                             kiconSize * kcircleInset,
                             kiconSize * kcircleSize,
                             kiconSize * kcircleSize));
    }

    QString glyph;
    switch (icon) {
    case QMessageBox::Warning:
    case QMessageBox::Critical:
        glyph = QStringLiteral("!");
        break;
    case QMessageBox::Information:
        glyph = QStringLiteral("i");
        break;
    case QMessageBox::Question:
        glyph = QStringLiteral("?");
        break;
    default:
        break;
    }
    if (!glyph.isEmpty()) {
        QFont font = p.font();
        font.setBold(true);
        font.setPixelSize(
            int(kiconSize
                * (icon == QMessageBox::Warning ? kglyphScaleTriangle
                                                : kglyphScaleCircle)));
        p.setFont(font);
        p.setPen(Qt::white);
        const QRectF textRect(0,
                              icon == QMessageBox::Warning ? kiconSize * 0.12
                                                           : 0,
                              kiconSize,
                              kiconSize);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, glyph);
    }

    p.end();
    return pm;
}

void apply_rounded_mask(QWidget* widget, int radiusPx)
{
    QPainterPath path;
    path.addRoundedRect(widget->rect(), radiusPx, radiusPx);
    widget->setMask(QRegion(path.toFillPolygon().toPolygon()));
}

namespace {

void apply_shadow(QWidget* widget, int blurRadius, int yOffset, int alpha)
{
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(blurRadius);
    shadow->setOffset(0, yOffset);
    shadow->setColor(QColor(0, 0, 0, alpha));
    widget->setGraphicsEffect(shadow);
}

} // namespace

void apply_dialog_shadow(QWidget* widget)
{
    constexpr int kblurRadius = 24;
    constexpr int kyOffset = 4;
    constexpr int kalpha = 150;
    apply_shadow(widget, kblurRadius, kyOffset, kalpha);
}

void apply_toolbar_shadow(QWidget* widget)
{
    constexpr int kblurRadius = 18;
    constexpr int kyOffset = 3;
    constexpr int kalpha = 140;
    apply_shadow(widget, kblurRadius, kyOffset, kalpha);
}

QPushButton* make_close_button(QWidget* parent, const char* objectName)
{
    constexpr int kbuttonSize = 22;

    auto* button = new QPushButton(QStringLiteral("×"), parent);
    button->setFixedSize(kbuttonSize, kbuttonSize);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(QString::fromLatin1(objectName));
    return button;
}

void apply_dialog_margins(QLayout* outer)
{
    constexpr int kleft = 20;
    constexpr int ktop = 14;
    constexpr int kright = 20;
    constexpr int kbottom = 16;
    outer->setContentsMargins(kleft, ktop, kright, kbottom);
}

} // namespace familiar::dialog_style

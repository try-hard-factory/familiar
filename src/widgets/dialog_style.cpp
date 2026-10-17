#include "dialog_style.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegion>
#include <QWidget>

namespace familiar::dialog_style {

QColor severity_color(QMessageBox::Icon icon, const QColor& accent)
{
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
    button->setMinimumWidth(76);
    button->setMinimumHeight(30);
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
                 accent.lighter(115).name(),
                 accent.darker(115).name()));
}

void style_secondary_button(QPushButton* button,
                            const QColor& text,
                            const QColor& border)
{
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumWidth(76);
    button->setMinimumHeight(30);
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
    hover.setAlpha(90);
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
        tri.moveTo(kiconSize * 0.5, kiconSize * 0.05);
        tri.lineTo(kiconSize * 0.96, kiconSize * 0.92);
        tri.lineTo(kiconSize * 0.04, kiconSize * 0.92);
        tri.closeSubpath();
        p.drawPath(tri);
    } else {
        p.drawEllipse(QRectF(kiconSize * 0.05,
                             kiconSize * 0.05,
                             kiconSize * 0.9,
                             kiconSize * 0.9));
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
            int(kiconSize * (icon == QMessageBox::Warning ? 0.38 : 0.48)));
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

} // namespace familiar::dialog_style

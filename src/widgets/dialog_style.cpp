#include "dialog_style.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegion>
#include <QWidget>

namespace familiar::dialog_style {

namespace {

constexpr int kIconSize = 40;

QColor severityColor(QMessageBox::Icon icon, const QColor& accent)
{
    switch (icon) {
    case QMessageBox::Warning:
        return QColor(0xf5, 0xa6, 0x23);
    case QMessageBox::Critical:
        return QColor(0xe5, 0x48, 0x4d);
    case QMessageBox::Information:
        return QColor(0x4a, 0x90, 0xd9);
    case QMessageBox::Question:
    default:
        return accent;
    }
}

} // namespace

QString panelStyleSheet(const char* className,
                        const QColor& background,
                        const QColor& border,
                        const QColor& text)
{
    return QStringLiteral("%1 {"
                         "  background-color: %2;"
                         "  border: 1px solid %3;"
                         "  border-radius: 10px;"
                         "}"
                         "QLabel { background: transparent; color: %4; }")
        .arg(QString::fromUtf8(className), background.name(), border.name(), text.name());
}

void stylePrimaryButton(QPushButton* button, const QColor& accent)
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
            .arg(accent.name(), accent.lighter(115).name(), accent.darker(115).name()));
}

void styleSecondaryButton(QPushButton* button,
                          const QColor& text,
                          const QColor& border)
{
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumWidth(76);
    button->setMinimumHeight(30);
    button->setStyleSheet(
        QStringLiteral("QPushButton {"
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

QPixmap severityIcon(QMessageBox::Icon icon, const QColor& accent, qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QColor bg = severityColor(icon, accent);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    if (icon == QMessageBox::Warning) {
        QPainterPath tri;
        tri.moveTo(kIconSize * 0.5, kIconSize * 0.05);
        tri.lineTo(kIconSize * 0.96, kIconSize * 0.92);
        tri.lineTo(kIconSize * 0.04, kIconSize * 0.92);
        tri.closeSubpath();
        p.drawPath(tri);
    } else {
        p.drawEllipse(QRectF(
            kIconSize * 0.05, kIconSize * 0.05, kIconSize * 0.9, kIconSize * 0.9));
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
            int(kIconSize * (icon == QMessageBox::Warning ? 0.38 : 0.48)));
        p.setFont(font);
        p.setPen(Qt::white);
        const QRectF textRect(
            0, icon == QMessageBox::Warning ? kIconSize * 0.12 : 0, kIconSize, kIconSize);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, glyph);
    }

    p.end();
    return pm;
}

void applyRoundedMask(QWidget* widget, int radiusPx)
{
    QPainterPath path;
    path.addRoundedRect(widget->rect(), radiusPx, radiusPx);
    widget->setMask(QRegion(path.toFillPolygon().toPolygon()));
}

} // namespace familiar::dialog_style

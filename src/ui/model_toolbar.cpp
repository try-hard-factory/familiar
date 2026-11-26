#include "model_toolbar.h"

#include "core/settingshandler.h"
#include "widgets/dialog_style.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

QIcon lock_icon(const QColor& color, qreal dpr, bool unlocked)
{
    constexpr int side = 18;
    QPixmap image(QSize(side, side) * dpr);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.8);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF shackle = unlocked ? QRectF(9, 2, 6, 9) : QRectF(6, 2, 6, 9);
    QPainterPath path;
    path.arcMoveTo(shackle, 0);
    path.arcTo(shackle, 0, 180);
    if (!unlocked) {
        path.lineTo(6, 9);
    }
    painter.drawPath(path);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(4, 8, 10, 8), 2, 2);
    return QIcon(image);
}

QString rgba(const QColor& color, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(alpha);
}

} // namespace

ModelToolbar::ModelToolbar(QWidget* parent)
    : QWidget(parent)
    , button_(new QToolButton(this))
{
    setAttribute(Qt::WA_StyledBackground);
    familiar::dialog_style::apply_toolbar_shadow(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 8, 5);
    button_->setFixedSize(30, 30);
    button_->setIconSize(QSize(18, 18));
    button_->setCheckable(true);
    button_->setAutoRaise(true);
    button_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(button_);

    connect(button_, &QToolButton::toggled, this, [this](bool editing) {
        editing_ = editing;
        update_button();
        if (item_) {
            emit edit_toggled(item_, editing);
        }
    });
    restyle_from_preset();
}

void ModelToolbar::attach(ModelItem* item)
{
    item_ = item;
    set_editing(false);
}

void ModelToolbar::set_editing(bool editing)
{
    editing_ = editing;
    const QSignalBlocker blocker(button_);
    button_->setChecked(editing);
    update_button();
}

void ModelToolbar::update_button()
{
    const QString label = editing_ ? tr("Lock 3D rotation")
                                   : tr("Unlock 3D rotation");
    button_->setToolTip(label);
    button_->setAccessibleName(label);
    const auto preset
        = SettingsHandler::get_instance()->get_current_color_preset();
    button_->setIcon(lock_icon(preset[EPresetsColorIdx::TextColor],
                               devicePixelRatioF(),
                               editing_));
}

void ModelToolbar::restyle_from_preset()
{
    const auto preset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& text = preset[EPresetsColorIdx::TextColor];
    const QColor& background = preset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = preset[EPresetsColorIdx::BorderColor];
    const QColor& selection = preset[EPresetsColorIdx::SelectionColor];
    setStyleSheet(QStringLiteral("ModelToolbar {"
                                 " background-color: %1;"
                                 " border: 1px solid %2;"
                                 " border-radius: 8px;"
                                 "}"
                                 "QToolButton {"
                                 " background: transparent;"
                                 " color: %3;"
                                 " border: none;"
                                 " border-radius: 5px;"
                                 "}"
                                 "QToolButton:hover { background-color: %4; }"
                                 "QToolButton:checked { background-color: %5; }"
                                 "QToolTip { background-color: %6; color: %3;"
                                 " border: 1px solid %2; }")
                      .arg(rgba(background, 245),
                           border.name(),
                           text.name(),
                           rgba(selection, 90),
                           rgba(selection, 170),
                           background.name()));
    update_button();
}

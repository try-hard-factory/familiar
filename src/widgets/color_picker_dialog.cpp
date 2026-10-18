#include "color_picker_dialog.h"
#include "dialog_style.h"

#include <core/settingshandler.h>

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHashFunctions>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPoint>
#include <QPushButton>
#include <QRegion>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWindow>
#include <Qt>
#include <QtMinMax>
#include <QtNumeric>
#include <QtPreprocessorSupport>
#include <QtTypes>

#include "utils/utils.h"

namespace {

constexpr int khandleRadius = 6;

// Colour-model bounds, not layout numbers: QColor channels and alpha are
// 0-255 bytes and hue runs 0-359 degrees around the wheel (QColor itself
// reports -1 for an achromatic colour - see set_color() below).
constexpr int kmaxChannel = 255;
constexpr qreal kmaxChannelF = 255.0;
constexpr int kmaxHue = 359;
constexpr int khueDegrees = 360;
// The hue strip is a gradient through the six primary/secondary stops,
// 60 degrees apart - 6 * 60 == the full 360.
constexpr int khueStops = 6;
constexpr int kdegreesPerHueStop = 60;
constexpr qreal kpercentMax = 100.0;

// Checkerboard drawn behind a translucent colour so the alpha reads.
constexpr int kcheckerTile = 12;
constexpr int kcheckerHalf = kcheckerTile / 2;
constexpr int kcheckerLight = 210;
constexpr int kcheckerDark = 150;

// Panel/swatch corner radii. Deliberately not shared with khandleRadius
// above, which happens to be the same number but means the picker's
// crosshair size.
constexpr int kpanelRadius = 6;
constexpr int kswatchRadius = 5;

// Both strips (hue and alpha) are the same thin bar.
constexpr int kstripHeight = 18;
constexpr int kstripMinWidth = 220;

// Dialog chrome.
constexpr int kdialogWidth = 280;
constexpr int kouterSpacing = 10;
constexpr int krowSpacing = 8;
constexpr int kpreviewSwatchSize = 28;
constexpr int khexDigits = 6;
constexpr int kpercentFieldWidth = 56;

QBrush checker_brush()
{
    // Same tiled-pixmap technique as every other drawn asset in this
    // app - built once per call is cheap enough (only paints while a
    // color dialog is actually open).
    QPixmap pm(kcheckerTile, kcheckerTile);
    pm.fill(QColor(kcheckerLight, kcheckerLight, kcheckerLight));
    QPainter p(&pm);
    p.fillRect(0,
               0,
               kcheckerHalf,
               kcheckerHalf,
               QColor(kcheckerDark, kcheckerDark, kcheckerDark));
    p.fillRect(kcheckerHalf,
               kcheckerHalf,
               kcheckerHalf,
               kcheckerHalf,
               QColor(kcheckerDark, kcheckerDark, kcheckerDark));
    p.end();
    return {pm};
}

} // namespace

// ============================================================================
// SvPicker
// ============================================================================
SvPicker::SvPicker(QWidget* parent)
    : QWidget(parent)
{
    constexpr int kpickerHeight = 170;
    constexpr int kpickerMinWidth = 220;
    setFixedHeight(kpickerHeight);
    setMinimumWidth(kpickerMinWidth);
    setCursor(Qt::CrossCursor);
}

void SvPicker::set_hue(int hue)
{
    hue_ = hue;
    update();
}

void SvPicker::set_sv(qreal s, qreal v)
{
    s_ = s;
    v_ = v;
    update();
}

void SvPicker::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath panel;
    panel.addRoundedRect(rect(), kpanelRadius, kpanelRadius);
    p.setClipPath(panel);

    QLinearGradient satGrad(0, 0, width(), 0);
    satGrad.setColorAt(0, QColor::fromHsv(hue_, 0, kmaxChannel));
    satGrad.setColorAt(1, QColor::fromHsv(hue_, kmaxChannel, kmaxChannel));
    p.fillRect(rect(), satGrad);

    QLinearGradient valGrad(0, 0, 0, height());
    valGrad.setColorAt(0, QColor(0, 0, 0, 0));
    valGrad.setColorAt(1, QColor(0, 0, 0, kmaxChannel));
    p.fillRect(rect(), valGrad);
    p.setClipping(false);

    const qreal hx = s_ * width();
    const qreal hy = (1.0 - v_) * height();
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(hx, hy), khandleRadius, khandleRadius);
    constexpr int kouterRingAlpha = 130;
    p.setPen(QPen(QColor(0, 0, 0, kouterRingAlpha), 1));
    p.drawEllipse(QPointF(hx, hy), khandleRadius + 1, khandleRadius + 1);
}

void SvPicker::pick(const QPoint& pos)
{
    s_ = qBound(0.0, pos.x() / qreal(width()), 1.0);
    v_ = 1.0 - qBound(0.0, pos.y() / qreal(height()), 1.0);
    update();
    emit sv_changed(s_, v_);
}

void SvPicker::mousePressEvent(QMouseEvent* event)
{
    pick(event->pos());
}

void SvPicker::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        pick(event->pos());
    }
}

// ============================================================================
// HueSlider
// ============================================================================
HueSlider::HueSlider(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kstripHeight);
    setMinimumWidth(kstripMinWidth);
    setCursor(Qt::PointingHandCursor);
}

void HueSlider::set_hue(int hue)
{
    hue_ = hue;
    update();
}

void HueSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath track;
    const qreal trackRadius = height() / 2.0;
    track.addRoundedRect(rect(), trackRadius, trackRadius);
    QLinearGradient grad(0, 0, width(), 0);
    for (int i = 0; i <= khueStops; ++i) {
        grad.setColorAt(i / qreal(khueStops),
                        QColor::fromHsv((i * kdegreesPerHueStop) % khueDegrees,
                                        kmaxChannel,
                                        kmaxChannel));
    }
    p.fillPath(track, grad);

    const qreal hx = hue_ / qreal(kmaxHue) * width();
    const qreal r = trackRadius - 1;
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(hx, trackRadius), r, r);
}

void HueSlider::pick(const QPoint& pos)
{
    hue_ = qBound(0, int(pos.x() / qreal(width()) * kmaxHue), kmaxHue);
    update();
    emit hue_changed(hue_);
}

void HueSlider::mousePressEvent(QMouseEvent* event)
{
    pick(event->pos());
}

void HueSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        pick(event->pos());
    }
}

// ============================================================================
// AlphaSlider
// ============================================================================
AlphaSlider::AlphaSlider(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kstripHeight);
    setMinimumWidth(kstripMinWidth);
    setCursor(Qt::PointingHandCursor);
}

void AlphaSlider::set_rgb(const QColor& rgb)
{
    rgb_ = rgb;
    update();
}

void AlphaSlider::set_alpha(int alpha)
{
    alpha_ = alpha;
    update();
}

void AlphaSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath track;
    const qreal trackRadius = height() / 2.0;
    track.addRoundedRect(rect(), trackRadius, trackRadius);
    p.setClipPath(track);
    p.fillRect(rect(), checker_brush());

    QLinearGradient grad(0, 0, width(), 0);
    QColor c0 = rgb_;
    c0.setAlpha(0);
    QColor c1 = rgb_;
    c1.setAlpha(kmaxChannel);
    grad.setColorAt(0, c0);
    grad.setColorAt(1, c1);
    p.fillRect(rect(), grad);
    p.setClipping(false);

    const qreal hx = alpha_ / kmaxChannelF * width();
    const qreal r = trackRadius - 1;
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(hx, trackRadius), r, r);
}

void AlphaSlider::pick(const QPoint& pos)
{
    alpha_ = qBound(0, int(pos.x() / qreal(width()) * kmaxChannel), kmaxChannel);
    update();
    emit alpha_changed(alpha_);
}

void AlphaSlider::mousePressEvent(QMouseEvent* event)
{
    pick(event->pos());
}

void AlphaSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        pick(event->pos());
    }
}

// ============================================================================
// SwatchRow
// ============================================================================
namespace {
constexpr int kswatchSize = 26;
constexpr int kswatchSpacing = 6;
} // namespace

SwatchRow::SwatchRow(bool withNone, const QColor& accent, QWidget* parent)
    : QWidget(parent)
    , withNone_(withNone)
    , accent_(accent)
{
    // The preset swatches, as data. Each literal is an RGB channel of a
    // chosen colour; naming them would restate the hex without adding
    // anything, same call as the palette tables in
    // core/settingshandler.cpp.
    // NOLINTBEGIN(readability-magic-numbers)
    colors_ = {
        QColor(0x1a, 0x1a, 0x1a),
        QColor(0x8b, 0x1a, 0x1a),
        QColor(0xd6, 0x3a, 0x3a),
        QColor(0xd1, 0x8b, 0x8b),
        QColor(0x3d, 0x8b, 0x3d),
        QColor(0x2b, 0x2b, 0x40),
        QColor(0xc4, 0x3d, 0xc4),
        QColor(0x3d, 0xa8, 0xd8),
    };
    // NOLINTEND(readability-magic-numbers)
    if (withNone_) {
        colors_.prepend(QColor(0, 0, 0, 0));
    }

    setFixedHeight(kswatchSize);
}

void SwatchRow::set_current(const QColor& color)
{
    current_ = color;
    update();
}

// Cell width divides whatever space the layout actually gave this
// widget evenly among every swatch, capped at the row's own height for
// the swatch itself (so cells narrower than kSwatchSize just draw a
// smaller square with more surrounding gap, rather than overflowing
// past the dialog's edge - fixed per-swatch pixel math clipped the last
// couple of swatches off the right side of the panel, confirmed
// visually).
QRectF SwatchRow::cell_rect(int index) const
{
    const int count = static_cast<int>(colors_.size());
    if (count == 0) {
        return {};
    }
    const qreal cellW = width() / qreal(count);
    const qreal size = qMin(cellW - kswatchSpacing, qreal(height()));
    const qreal x = (index * cellW) + ((cellW - size) / 2.0);
    const qreal y = (height() - size) / 2.0;
    return {x, y, size, size};
}

int SwatchRow::swatch_at(const QPoint& pos) const
{
    const int count = static_cast<int>(colors_.size());
    if (count == 0) {
        return -1;
    }
    const int idx = int(pos.x() / (width() / qreal(count)));
    if (idx < 0 || idx >= count) {
        return -1;
    }
    return cell_rect(idx).contains(pos) ? idx : -1;
}

void SwatchRow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < colors_.size(); ++i) {
        const QRectF r = cell_rect(i);
        const QColor& c = colors_[i];
        if (c.alpha() == 0) {
            // "None" swatch - checkerboard + a diagonal slash, same
            // "this means transparent" convention as most design tools.
            constexpr int kslashRed = 200;
            constexpr int kslashGreenBlue = 60;
            constexpr int kslashWidth = 2;
            QPainterPath clip;
            clip.addRoundedRect(r, kswatchRadius, kswatchRadius);
            p.save();
            p.setClipPath(clip);
            p.fillRect(r, checker_brush());
            p.setPen(QPen(QColor(kslashRed, kslashGreenBlue, kslashGreenBlue),
                          kslashWidth));
            p.drawLine(r.topLeft(), r.bottomRight());
            p.restore();
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(r, kswatchRadius, kswatchRadius);
        }

        const bool isCurrent = current_.isValid()
                               && current_.rgba() == c.rgba();
        if (isCurrent) {
            // Drawn just outside the swatch so it reads as a ring around
            // it rather than a border on it.
            constexpr qreal kringWidth = 2.0;
            constexpr int kringOutset = 2;
            QPen ring(accent_);
            ring.setWidthF(kringWidth);
            p.setPen(ring);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-kringOutset,
                                         -kringOutset,
                                         kringOutset,
                                         kringOutset),
                              kpanelRadius,
                              kpanelRadius);
        }
    }
}

void SwatchRow::mousePressEvent(QMouseEvent* event)
{
    const int idx = swatch_at(event->pos());
    if (idx >= 0) {
        emit swatch_picked(colors_[idx]);
    }
}

// ============================================================================
// ColorPickerDialog
// ============================================================================
ColorPickerDialog::ColorPickerDialog(QWidget* parent,
                                     const QColor& initial,
                                     const QString& title,
                                     bool withAlpha)
    : QDialog(parent)
    , current_(initial.isValid() ? initial : Qt::black)
    , withAlpha_(withAlpha)
{
    if (!withAlpha_) {
        current_.setAlpha(kmaxChannel);
    }

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(title);
    setFixedWidth(kdialogWidth);

    familiar::dialog_style::apply_dialog_shadow(this);

    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::TextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::BorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::SelectionColor];

    auto* outer = new QVBoxLayout(this);
    // Tighter than dialog_style::apply_dialog_margins() - this is a dense
    // tool dialog, not a message dialog.
    constexpr QMargins kouterMargins{16, 12, 16, 16};
    outer->setContentsMargins(kouterMargins);
    outer->setSpacing(kouterSpacing);

    auto* topRow = new QHBoxLayout();
    auto* titleLabel = new QLabel(title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    topRow->addWidget(titleLabel, 1);

    auto* closeBtn = familiar::dialog_style::make_close_button(this,
                                                               "cpdCloseBtn");
    connect(closeBtn, &QPushButton::clicked, this, &ColorPickerDialog::reject);
    topRow->addWidget(closeBtn);
    outer->addLayout(topRow);

    svPicker_ = new SvPicker(this);
    outer->addWidget(svPicker_);

    hueSlider_ = new HueSlider(this);
    outer->addWidget(hueSlider_);

    if (withAlpha_) {
        alphaSlider_ = new AlphaSlider(this);
        outer->addWidget(alphaSlider_);
    }

    auto* fieldsRow = new QHBoxLayout();
    fieldsRow->setSpacing(krowSpacing);

    previewSwatch_ = new QLabel(this);
    previewSwatch_->setFixedSize(kpreviewSwatchSize, kpreviewSwatchSize);
    fieldsRow->addWidget(previewSwatch_);

    hexEdit_ = new QLineEdit(this);
    hexEdit_->setMaxLength(khexDigits);
    hexEdit_->setPlaceholderText(QStringLiteral("RRGGBB"));
    fieldsRow->addWidget(hexEdit_, 1);

    auto* copyBtn = new QPushButton(QStringLiteral("⧉"), this);
    copyBtn->setFixedSize(kpreviewSwatchSize, kpreviewSwatchSize);
    copyBtn->setCursor(Qt::PointingHandCursor);
    copyBtn->setToolTip(tr("Copy hex value"));
    connect(copyBtn, &QPushButton::clicked, this, [this, copyBtn] {
        qApp->clipboard()->setText(current_.name(QColor::HexRgb));

        QToolTip::showText(copyBtn->mapToGlobal(
                               QPoint(copyBtn->width() / 2, copyBtn->height())),
                           tr("Copied to clipboard!"),
                           copyBtn);
    });
    fieldsRow->addWidget(copyBtn);

    if (withAlpha_) {
        percentEdit_ = new QLineEdit(this);
        percentEdit_->setFixedWidth(kpercentFieldWidth);
        percentEdit_->setAlignment(Qt::AlignRight);
        fieldsRow->addWidget(percentEdit_);
    }
    outer->addLayout(fieldsRow);

    swatchRow_ = new SwatchRow(withAlpha_, accent, this);
    outer->addWidget(swatchRow_);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(krowSpacing);
    buttonRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    familiar::dialog_style::style_secondary_button(cancelBtn, textColor, border);
    connect(cancelBtn, &QPushButton::clicked, this, &ColorPickerDialog::reject);
    buttonRow->addWidget(cancelBtn);
    auto* okBtn = new QPushButton(tr("OK"), this);
    familiar::dialog_style::style_primary_button(okBtn, accent);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &ColorPickerDialog::accept);
    buttonRow->addWidget(okBtn);
    outer->addLayout(buttonRow);

    connect(svPicker_, &SvPicker::sv_changed, this, [this](qreal s, qreal v) {
        const QColor c = QColor::fromHsv(hueSlider_->hue(),
                                         int(s * 255),
                                         int(v * 255),
                                         current_.alpha());
        set_color(c, svPicker_);
    });
    connect(hueSlider_, &HueSlider::hue_changed, this, [this](int hue) {
        int h = 0;
        int s = 0;
        int v = 0;
        int a = 0;
        current_.getHsv(&h, &s, &v, &a);
        set_color(QColor::fromHsv(hue, s, v, a), hueSlider_);
    });
    if (alphaSlider_) {
        connect(alphaSlider_, &AlphaSlider::alpha_changed, this, [this](int a) {
            QColor c = current_;
            c.setAlpha(a);
            set_color(c, alphaSlider_);
        });
    }
    connect(hexEdit_, &QLineEdit::editingFinished, this, [this] {
        // No leading "#" in the field itself - QColor's own
        // string parsing needs it, so it's added back here rather than
        // shown to the user.
        QColor c(QStringLiteral("#") + hexEdit_->text());
        if (!c.isValid()) {
            return;
        }
        c.setAlpha(current_.alpha());
        set_color(c, hexEdit_);
    });
    if (percentEdit_) {
        connect(percentEdit_, &QLineEdit::editingFinished, this, [this] {
            bool ok = false;
            const int pct = qBound(0, percentEdit_->text().toInt(&ok), 100);
            if (!ok) {
                return;
            }
            QColor c = current_;
            c.setAlpha(qRound(pct * kmaxChannelF / kpercentMax));
            set_color(c, percentEdit_);
        });
    }
    connect(swatchRow_, &SwatchRow::swatch_picked, this, [this](QColor c) {
        if (!withAlpha_) {
            c.setAlpha(kmaxChannel);
        }
        set_color(c, swatchRow_);
    });

    set_color(current_, nullptr);

    setStyleSheet(
        familiar::dialog_style::panel_style_sheet("ColorPickerDialog",
                                                  background,
                                                  border,
                                                  textColor,
                                                  /*radiusPx=*/0)
        + familiar::dialog_style::close_button_style_sheet("cpdCloseBtn",
                                                           textColor,
                                                           accent)
        + QStringLiteral("QLineEdit {"
                         "  background-color: rgba(0,0,0,20);"
                         "  color: %1;"
                         "  border: 1px solid %2;"
                         "  border-radius: 4px;"
                         "  padding: 4px 6px;"
                         "}")
              .arg(textColor.name(), border.name()));

    centered_widget(parent ? parent : this, this);
}

void ColorPickerDialog::set_color(const QColor& color, QObject* source)
{
    current_ = color;

    int h = 0;
    int s = 0;
    int v = 0;
    color.getHsv(&h, &s, &v);
    // QColor::hue() (and getHsv()'s h) is -1 for achromatic colors
    // (saturation 0 - pure black/white/gray) since hue is meaningless
    // there - keep whatever hue the slider was already showing instead
    // of snapping it to 0, so e.g. dragging value down to black and
    // back up doesn't silently reset a chosen hue.
    const int effectiveHue = h < 0 ? hueSlider_->hue() : h;

    if (source != hueSlider_) {
        hueSlider_->set_hue(effectiveHue);
    }
    if (source != svPicker_) {
        svPicker_->set_sv(s / kmaxChannelF, v / kmaxChannelF);
    }
    // Always resync, regardless of source - setHue() just repaints, it
    // doesn't emit anything, so this can't cause a feedback loop, and
    // the SV square's gradient needs the current hue to render at all.
    svPicker_->set_hue(effectiveHue);

    if (alphaSlider_) {
        alphaSlider_->set_rgb(color);
        if (source != alphaSlider_) {
            alphaSlider_->set_alpha(color.alpha());
        }
    }
    if (source != hexEdit_) {
        hexEdit_->setText(color.name(QColor::HexRgb).mid(1));
    }
    if (percentEdit_ && source != percentEdit_) {
        percentEdit_->setText(
            QString::number(qRound(color.alpha() * kpercentMax / kmaxChannelF))
            + QStringLiteral("%"));
    }

    swatchRow_->set_current(color);

    QPixmap pm(previewSwatch_->size());
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(pm.rect(), kswatchRadius, kswatchRadius);
        p.setClipPath(clip);
        p.fillRect(pm.rect(), checker_brush());
        p.fillRect(pm.rect(), color);
    }
    previewSwatch_->setPixmap(pm);

    emit color_changed(color);
}

void ColorPickerDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

QColor show_color_picker_dialog(QWidget* parent,
                                const QColor& initial,
                                const QString& title,
                                bool withAlpha)
{
    ColorPickerDialog dlg(parent, initial, title, withAlpha);
    if (dlg.exec() != QDialog::Accepted) {
        return {};
    }
    return dlg.selected_color();
}

#include "colors_widget.h"
#include <core/settingshandler.h>

#include "log/log.h"
using namespace familiar::log;
#include "widgets/dialogs.h"
#include <ui/extendedslider.h>
#include <ui/presetsave_window.h>
#include <utils/utils.h>
#include <widgets/color_picker_dialog.h>
#include <widgets/settings_style.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kSwatchDiameter = 30;

// Fixed mid-gray, not settings_style::palette().border (#D8D8D8) - that
// value is tuned for hairlines against this window's own fixed white
// background, too close to white to read as an outline against a swatch
// whose fill also happens to be light/white (e.g. a default "Background
// color" preset value) - the ring all but disappeared (confirmed
// visually). This is the swatch's own edge against the page, not a
// window-chrome hairline, so it gets its own darker constant instead of
// reusing that one.
const QColor kSwatchBorderColor(0xB0, 0xB0, 0xB0);

QIcon makeSwatchIcon(const QColor& color, qreal dpr)
{
    QPixmap pm(QSize(kSwatchDiameter, kSwatchDiameter) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen border(kSwatchBorderColor);
    border.setWidthF(1.5);
    p.setPen(border);
    p.setBrush(color);
    p.drawEllipse(
        QRectF(1.0, 1.0, kSwatchDiameter - 2.0, kSwatchDiameter - 2.0));
    p.end();

    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

} // namespace


ColorsWidget::ColorsWidget(QWidget* parent)
    : QWidget(parent)
    , layout_(new QVBoxLayout(this))
    , opacitySlider_(new ExtendedSlider())
    , header_layout_(new QHBoxLayout())
    , body_layout_(new QHBoxLayout())
    , slider_layout_(new QHBoxLayout())
    , bottom_layout_(new QHBoxLayout())
{
    layout_->setAlignment(Qt::AlignTop);

    labelsInit();
    presetsInit();
    colorInit();
    sliderInit();
    saveResetBtnsInit();

    layout_->addLayout(header_layout_);
    layout_->addLayout(body_layout_);
    layout_->addSpacing(50);
    layout_->addLayout(slider_layout_);
    layout_->addSpacing(50);
    layout_->addLayout(bottom_layout_);

    setLayout(layout_);
}


ColorsWidget::~ColorsWidget()
{
    delete layout_;
}

void ColorsWidget::labelsInit()
{
    auto* presets_lbl = new QLabel(tr("Presets"));
    presets_lbl->setAlignment(Qt::AlignLeft);
    auto* colors_lbl = new QLabel(tr("Colors"));
    colors_lbl->setAlignment(Qt::AlignCenter);
    header_layout_->addWidget(presets_lbl);
    header_layout_->addWidget(colors_lbl);
}


void ColorsWidget::presetsInit()
{
    auto* presets_layout = new QVBoxLayout();
    presets_layout->setAlignment(Qt::AlignLeft);
    presets_layout->setSpacing(4);

    presetButtons_ = new QButtonGroup(this);
    presetButtons_->setExclusive(true);

    struct PresetBtn
    {
        QString label;
        EPresets preset;
    };
    const QList<PresetBtn> presets = {
        {tr("Dark"), EPresets::kDarkPreset},
        {tr("Light"), EPresets::kLightPreset},
        {tr("Custom 1"), EPresets::kCustom1},
        {tr("Custom 2"), EPresets::kCustom2},
        {tr("Custom 3"), EPresets::kCustom3},
        {tr("Custom 4"), EPresets::kCustom4},
    };

    for (const PresetBtn& preset : presets) {
        auto* btn = new QPushButton(preset.label, this);
        // Same #categoryButton chrome as the sidebar's own
        // CategoryNavButton (ui/settings_window.cpp): idle/hover/checked
        // fill. No custom paintEvent needed here the way that class
        // needs one - preset names never need the bold search-match
        // rich text CategoryNavButton exists for, so plain
        // QPushButton::setText() (which sidebarButtonStyleSheet() alone
        // can already color via the window's inherited "* { color }"
        // cascade) is enough.
        btn->setObjectName(QStringLiteral("categoryButton"));
        // sidebarButtonStyleSheet() alone has no padding/min-height -
        // CategoryNavButton (ui/settings_window.cpp) gets away with that
        // because it hand-paints its own text instead of relying on
        // QPushButton::setText()'s native layout, so it never needed
        // breathing room from QSS. This button does rely on native text
        // layout, so without this "Custom 1" etc. came out clipped
        // against the button edges (confirmed visually). NOT
        // CategoryNavButton's own 38px min-height though - six of these
        // stacked in this narrower column at that height ran into each
        // other with zero visible gap (confirmed visually) - same
        // min-height/padding as filledButtonStyleSheet()'s buttons
        // instead (Restore Defaults etc., which read at a normal size).
        // Also overrides sidebarButtonStyleSheet()'s own "text-align:
        // left" (matches the sidebar's icon+label list convention) -
        // this column is short standalone preset names, not a list of
        // longer category labels, so centered reads better. Same
        // selector, appended after the base sheet in this one combined
        // string - later wins on a tie, no separate stylesheet needed.
        btn->setStyleSheet(familiar::settings_style::sidebarButtonStyleSheet()
                           + QStringLiteral("QPushButton#categoryButton {"
                                            "  padding: 4px 14px;"
                                            "  text-align: center;"
                                            "}"));
        btn->setMinimumHeight(22);
        btn->setMinimumWidth(110);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);

        const EPresets value = preset.preset;
        connect(btn, &QPushButton::clicked, this, [value]() {
            SettingsHandler::getInstance()->setCurrentPreset(value);
            emit SettingsHandler::getInstance() -> presetsChanged();
        });

        presetButtons_->addButton(btn, static_cast<int>(preset.preset));
        presets_layout->addWidget(btn);
    }

    if (QAbstractButton* checked = presetButtons_->button(
            SettingsHandler::getInstance()->currentPreset())) {
        checked->setChecked(true);
    }

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::presetsChanged,
            this,
            &ColorsWidget::updateComponents);

    body_layout_->addLayout(presets_layout);
}


void ColorsWidget::colorInit()
{
    auto* colors_layout = new QVBoxLayout();
    colors_layout->setAlignment(Qt::AlignRight);

    struct ColorRow
    {
        QString label;
        EPresetsColorIdx idx;
    };
    const QList<ColorRow> rows = {
        {tr("Background color: "), EPresetsColorIdx::kBackgroundColor},
        {tr("Canvas color: "), EPresetsColorIdx::kCanvasColor},
        {tr("Border color: "), EPresetsColorIdx::kBorderColor},
        {tr("UI Text Color: "), EPresetsColorIdx::kTextColor},
        {tr("Selection color: "), EPresetsColorIdx::kSelectionColor},
    };

    for (const ColorRow& row : rows) {
        auto* row_layout = new QHBoxLayout();
        row_layout->setAlignment(Qt::AlignRight);

        auto* color_lbl = new QLabel(row.label);
        color_lbl->setAlignment(Qt::AlignRight);
        row_layout->addWidget(color_lbl);

        auto* swatch = new QToolButton(this);
        swatch->setFixedSize(kSwatchDiameter, kSwatchDiameter);
        swatch->setAutoRaise(true);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setStyleSheet(QStringLiteral(
            "QToolButton { border: none; background: transparent; }"));
        row_layout->addWidget(swatch);

        colorSwatches_[row.idx] = swatch;
        const EPresetsColorIdx idx = row.idx;
        connect(swatch, &QToolButton::clicked, this, [this, idx]() {
            pickColor_(idx);
        });

        colors_layout->addLayout(row_layout);
    }

    for (int i = 0; i < EPresetsColorIdx::kAllIdx; ++i) {
        refreshSwatch_(static_cast<EPresetsColorIdx>(i));
    }

    body_layout_->addLayout(colors_layout);
}

void ColorsWidget::pickColor_(EPresetsColorIdx idx)
{
    auto* settings = SettingsHandler::getInstance();
    const auto preset = settings->getCurrentColorPreset();
    const QColor oldColor = preset[idx];

    const QMap<EPresetsColorIdx, QString> titles = {
        {EPresetsColorIdx::kBackgroundColor, tr("Background color")},
        {EPresetsColorIdx::kCanvasColor, tr("Canvas color")},
        {EPresetsColorIdx::kBorderColor, tr("Border color")},
        {EPresetsColorIdx::kTextColor, tr("UI Text Color")},
        {EPresetsColorIdx::kSelectionColor, tr("Selection color")},
    };

    ColorPickerDialog dialog(this, oldColor, titles.value(idx));
    connect(&dialog,
            &ColorPickerDialog::colorChanged,
            this,
            [this, settings, idx](QColor c) {
                auto p = settings->getCurrentColorPreset();
                p[idx] = c;
                settings->setCurrentColorPreset(p);
                refreshSwatch_(idx);
                emit SettingsHandler::getInstance() -> settingsChanged();
            });

    if (dialog.exec() != QDialog::Accepted) {
        // Cancelled - revert the live preview colorChanged() applied
        // above while dragging, same convention GroupToolbar's fill-color
        // button uses (ui/group_toolbar.cpp).
        auto p = settings->getCurrentColorPreset();
        p[idx] = oldColor;
        settings->setCurrentColorPreset(p);
        refreshSwatch_(idx);
        emit SettingsHandler::getInstance() -> settingsChanged();
    }
}

void ColorsWidget::refreshSwatch_(EPresetsColorIdx idx)
{
    QToolButton* swatch = colorSwatches_[idx];
    if (!swatch) {
        return;
    }
    const auto preset = SettingsHandler::getInstance()->getCurrentColorPreset();
    swatch->setIcon(makeSwatchIcon(preset[idx], devicePixelRatioF()));
}


void ColorsWidget::sliderInit()
{
    // TODOLATER
    auto* settings = SettingsHandler::getInstance();
    // slider init
    opacitySlider_->setFocusPolicy(Qt::NoFocus);
    opacitySlider_->setOrientation(Qt::Horizontal);
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setCursor(Qt::PointingHandCursor);
    opacitySlider_->setStyleSheet(familiar::settings_style::sliderStyleSheet());
    slider_layout_->setAlignment(Qt::AlignBottom);
    slider_layout_->addWidget(new QLabel(QStringLiteral("Master opacity:")));
    slider_layout_->addWidget(opacitySlider_);

    opacitySlider_->setMapedValue(0, settings->getCurrentOpacity(), 255);
    connect(opacitySlider_, &ExtendedSlider::valueChanged, [this]() {
        FLOG_DEBUG(Ch::UI,
                   "Master opacity from settings = {}",
                   debugString(SettingsHandler::getInstance()->masterOpacity()));
        SettingsHandler::getInstance()->setCurrentOpacity(
            opacitySlider_->mappedValue(0, 255));
        //qDebug()<<"Opacity: "<<opacitySlider_->mappedValue(0, 255);
        emit SettingsHandler::getInstance() -> settingsChanged();
    });
}


void ColorsWidget::saveResetBtnsInit()
{
    // Same filled-gray-box chrome as the window's own bottom row
    // (Restore Defaults/Import/Export, ui/settings_window.cpp) - not
    // dialog_style::styleSecondaryButton()'s outline look, which is for
    // separate modal dialogs, not buttons living inside this window.
    QPushButton* save_to_preset_btn = new QPushButton(tr("Save to preset"),
                                                      this);
    save_to_preset_btn->setStyleSheet(
        familiar::settings_style::filledButtonStyleSheet());
    connect(save_to_preset_btn,
            &QPushButton::clicked,
            this,
            &ColorsWidget::showPresetSaveWindow);

    // "Reset to default" used to live here too - redundant now that the
    // window's own Restore Defaults button (settings_window.cpp) opens
    // RestoreDefaultsDialog with a Colors category covering exactly the
    // same reset, per-category checkbox and all. Max's call.
    bottom_layout_->addWidget(save_to_preset_btn);
}

void ColorsWidget::showPresetSaveWindow()
{
    PresetSaveWindow* widget = new PresetSaveWindow(parentWidget());
    widget->show();
    centered_widget(this, widget);
}


void ColorsWidget::updateComponents()
{
    auto* settings = SettingsHandler::getInstance();

    for (int i = 0; i < EPresetsColorIdx::kAllIdx; ++i) {
        refreshSwatch_(static_cast<EPresetsColorIdx>(i));
    }

    opacitySlider_->setMapedValue(0, settings->getCurrentOpacity(), 255);

    if (QAbstractButton* checked = presetButtons_->button(
            settings->currentPreset())) {
        checked->setChecked(true);
    }

    emit SettingsHandler::getInstance() -> settingsChanged();
}

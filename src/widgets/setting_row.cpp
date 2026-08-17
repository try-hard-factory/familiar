#include "setting_row.h"

#include "core/settings.h"
#include <widgets/flat_checkbox.h>
#include <widgets/flat_combobox.h>
#include <widgets/flat_spinbox.h>
#include <widgets/setting_descriptions.h>
#include <widgets/settings_style.h>

#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>

#include "log/log.h"
using namespace familiar::log;
namespace {
constexpr char CHANGED_SYMBOL[] = "✎";

constexpr int kControlWidth = 200;
constexpr int kInfoPopupWidth = 320;
} // namespace

// ─── SettingInfoPopup ───────────────────────────────────────────────────────

SettingInfoPopup::SettingInfoPopup(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    // Never intercepts a click/double-click meant for whatever's under
    // it - critical for the Ctrl+double-click-to-reset gesture this
    // popup's own footer hint describes: the popup often visually
    // overlaps the row it's attached to.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // A plain QWidget (this isn't a QFrame) doesn't auto-paint a QSS
    // background-color at all without this - without it the panel fell
    // back to a solid black plate (confirmed visually) instead of white.
    // Same attribute every dialog_style::panelStyleSheet() consumer sets
    // (ChangeOpacityDialog etc., widgets/dialogs.h) - missed here at
    // first.
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(kInfoPopupWidth);

    // Square, not dialog_style::panelStyleSheet()'s usual rounded panel
    // (border-radius baked into that shared helper, paired with
    // applyRoundedMask() to clip the actual window shape to match) -
    // this hit the same square-window-under-a-rounded-QSS-paint artifact
    // this whole app's dialogs already ran into once (see
    // SettingsWindow's own history: rounded corners reverted to square
    // for the same reason), so this popup just skips rounding outright
    // rather than reaching for applyRoundedMask() again.
    const auto& sp = familiar::settings_style::palette();
    setStyleSheet(
        QStringLiteral("SettingInfoPopup {"
                       "  background-color: %1;"
                       "  border: 1px solid %2;"
                       "}"
                       "QLabel { background: transparent; color: %3; }")
            .arg(sp.background.name(), sp.border.name(), sp.text.name()));

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 110));
    setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 10);
    layout->setSpacing(8);

    titleLabel_ = new QLabel(this);
    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    layout->addWidget(titleLabel_);

    bodyLabel_ = new QLabel(this);
    bodyLabel_->setWordWrap(true);
    bodyLabel_->setTextFormat(Qt::RichText);
    layout->addWidget(bodyLabel_);

    defaultLabel_ = new QLabel(this);
    defaultLabel_->setStyleSheet(
        QStringLiteral("color: %1;").arg(sp.mutedText.name()));
    layout->addWidget(defaultLabel_);

    hintLabel_ = new QLabel(tr("Ctrl + Dbl click to restore default"), this);
    QFont hintFont = hintLabel_->font();
    hintFont.setPointSize(hintFont.pointSize() - 1);
    hintLabel_->setFont(hintFont);
    hintLabel_->setStyleSheet(
        QStringLiteral("color: %1;").arg(sp.mutedText.name()));
    hintLabel_->setAlignment(Qt::AlignRight);
    layout->addWidget(hintLabel_);
}

void SettingInfoPopup::setContent(const QString& title,
                                  const QString& bodyHtml,
                                  const QString& defaultText,
                                  bool showResetHint)
{
    titleLabel_->setText(title);
    bodyLabel_->setText(bodyHtml);
    bodyLabel_->setVisible(!bodyHtml.isEmpty());
    defaultLabel_->setText(defaultText);
    defaultLabel_->setVisible(!defaultText.isEmpty());
    hintLabel_->setVisible(showResetHint);
    adjustSize();
}

// ─── HoverInfoLabel ─────────────────────────────────────────────────────────

HoverInfoLabel::HoverInfoLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent)
    , title_(text)
{
    // No native hand-cursor for "this has a tooltip" in Qt - WhatsThis
    // is the closest stock cursor that reads as "hover for more info".
    setCursor(Qt::WhatsThisCursor);
}

void HoverInfoLabel::setInfoText(const QString& html)
{
    infoHtml_ = html;
    // Kept in sync even though the native tooltip never actually
    // renders any more (event() below intercepts QEvent::ToolTip before
    // QLabel's own handling gets a chance) - still what screen readers/
    // anything else reading QWidget::toolTip() sees.
    setToolTip(html);
}

void HoverInfoLabel::setDefaultText(const QString& text)
{
    defaultText_ = text;
}

void HoverInfoLabel::setShowResetHint(bool show)
{
    showResetHint_ = show;
}

bool HoverInfoLabel::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        if (!infoHtml_.isEmpty()) {
            if (!popup_) {
                popup_ = new SettingInfoPopup(window());
            }
            popup_->setContent(title_, infoHtml_, defaultText_, showResetHint_);

            QPoint pos = mapToGlobal(QPoint(0, height() + 4));
            // Clamp to the screen this label is actually on, same idea
            // as any other popup that can open near a screen edge - a
            // 320px-wide panel opened from a row near the right edge of
            // a narrow Settings window would otherwise run off-screen.
            if (QScreen* screen = this->screen()) {
                const QRect avail = screen->availableGeometry();
                popup_->adjustSize();
                pos.setX(qBound(avail.left(),
                                pos.x(),
                                avail.right() - popup_->width()));
                pos.setY(qBound(avail.top(),
                                pos.y(),
                                avail.bottom() - popup_->height()));
            }
            popup_->move(pos);
            popup_->show();
        }
        return true;
    }
    return QLabel::event(event);
}

void HoverInfoLabel::leaveEvent(QEvent* event)
{
    QLabel::leaveEvent(event);
    if (popup_) {
        popup_->hide();
    }
}

// ─── SettingRowBase ─────────────────────────────────────────────────────────

SettingRowBase::SettingRowBase(const QString& label,
                               const QString& key,
                               QWidget* parent)
    : QWidget(parent)
    , key_(key)
    , baseLabel_(label)
    , hbox_(new QHBoxLayout(this))
    , label_(new HoverInfoLabel(label, this))
{
    // Searchable by the search box's applyGroupFilter() (ui/
    // settings_window.cpp) - raw label, no "✎" changed-marker.
    setObjectName(label);
    hbox_->setContentsMargins(0, 4, 0, 4);
    label_->setInfoText(familiar::setting_descriptions::forSettingsKey(key));
    hbox_->addWidget(label_);
    hbox_->addStretch(1);

    connect(&SettingsEvents::instance(),
            &SettingsEvents::restoreDefaults,
            this,
            &SettingRowBase::onRestoreDefaults);

    updateLabel();
}

void SettingRowBase::updateLabel()
{
    FamSettings settings;
    QString text = baseLabel_;
    if (settings.valueChanged(key_)) {
        text += QStringLiteral(" ") + QString::fromUtf8(CHANGED_SYMBOL);
    }
    label_->setText(text);
}

void SettingRowBase::onValueChanged(const QVariant& value)
{
    if (ignoreValueChanged_) {
        return;
    }

    FamSettings settings;
    const QVariant converted = convertValueFromQt(value);
    if (converted != settings.valueOrDefault(key_)) {
        settings.setValue(key_, converted);
        updateLabel();
    }
}

void SettingRowBase::onRestoreDefaults()
{
    FamSettings settings;
    ignoreValueChanged_ = true;
    setValue(settings.valueOrDefault(key_));
    ignoreValueChanged_ = false;
    updateLabel();
}

QString SettingRowBase::defaultValueDisplayText() const
{
    return FamSettings().valueOrDefault(key_).toString();
}

void SettingRowBase::refreshInfoPopup()
{
    label_->setDefaultText(tr("Default: %1").arg(defaultValueDisplayText()));
    label_->setShowResetHint(true);
}

void SettingRowBase::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        FamSettings settings;
        // Unlike onRestoreDefaults() above (the page-wide button already
        // cleared the whole JSON group before emitting
        // SettingsEvents::restoreDefaults(), so that path only needs to
        // resync the widget's displayed value) - this is the one place
        // that actually removes just THIS field's stored override.
        // FamSettings::remove() also fires its postSaveCallback with the
        // resulting default, so real runtime effects (e.g.
        // QImageReader::setAllocationLimit() for Maximum Image Size)
        // re-apply immediately too, not just the display.
        settings.remove(key_);
        ignoreValueChanged_ = true;
        setValue(settings.valueOrDefault(key_));
        ignoreValueChanged_ = false;
        updateLabel();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

// ─── ComboSettingRow ────────────────────────────────────────────────────────

ComboSettingRow::ComboSettingRow(const QString& label,
                                 const QString& key,
                                 const QList<ComboOption>& options,
                                 QWidget* parent)
    : SettingRowBase(label, key, parent)
    , input_([&] {
        const auto& sp = familiar::settings_style::palette();
        FLOG_DEBUG(Ch::UI, "popupItemHover {}", sp.popupItemHover);
        return new FlatComboBox(sp.chipBackground,
                                sp.text,
                                sp.popupItemHover,
                                sp.mutedText,
                                this);
    }())
    , options_(options)
{
    FamSettings settings;
    for (const ComboOption& opt : options_) {
        input_->addItem(opt.label);
    }
    input_->setFixedWidth(kControlWidth);
    // setValue() (via setCurrentIndex) happens before this connect(), so
    // it can't fire onValueChanged() with nothing listening yet - no
    // ignoreValueChanged_ guard needed.
    setValue(settings.valueOrDefault(key_));
    hbox_->addWidget(input_);
    ignoreValueChanged_ = false;

    connect(input_,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (index >= 0 && index < options_.size()) {
                    onValueChanged(options_[index].value);
                }
            });

    // options_ (used by defaultValueDisplayText() below) is fully set by
    // now - see that method's own comment for why this can't happen from
    // SettingRowBase's constructor instead.
    refreshInfoPopup();
}

void ComboSettingRow::setValue(const QVariant& value)
{
    const QString str = value.toString();
    for (int i = 0; i < options_.size(); ++i) {
        if (options_[i].value == str) {
            input_->setCurrentIndex(i);
            return;
        }
    }
}

QString ComboSettingRow::defaultValueDisplayText() const
{
    const QString def = FamSettings().valueOrDefault(key_).toString();
    for (const ComboOption& opt : options_) {
        if (opt.value == def) {
            return opt.label;
        }
    }
    return def;
}

void ComboSettingRow::setControlEnabled(bool enabled)
{
    input_->setEnabled(enabled);
}

// ─── CheckboxSettingRow ─────────────────────────────────────────────────────

CheckboxSettingRow::CheckboxSettingRow(const QString& label,
                                       const QString& key,
                                       QWidget* parent)
    : SettingRowBase(label, key, parent)
    , input_([&] {
        const auto& sp = familiar::settings_style::palette();
        return new FlatCheckBox(QString(),
                                sp.text,
                                sp.text,
                                sp.navSelectedBg,
                                this);
    }())
{
    // QCheckBox::sizeHint() with an empty label came out degenerate (a
    // sliver, not a square) - the indicator+spacing+text-width math it
    // does apparently doesn't cope with zero-length text the way it
    // does with save_all_dialog.cpp's FlatCheckBox usage (always has a
    // real filename). Pin an explicit size instead of trusting it.
    input_->setFixedSize(22, 22);

    FamSettings settings;
    setValue(settings.valueOrDefault(key_));
    hbox_->addWidget(input_);
    ignoreValueChanged_ = false;

    connect(input_,
            &QCheckBox::checkStateChanged,
            this,
            [this](Qt::CheckState state) {
                onValueChanged(QVariant::fromValue(state));
                emit toggled(state == Qt::Checked);
            });

    refreshInfoPopup();
}

void CheckboxSettingRow::setValue(const QVariant& value)
{
    input_->setChecked(value.toBool());
}

QVariant CheckboxSettingRow::convertValueFromQt(const QVariant& value)
{
    return value.value<Qt::CheckState>() == Qt::Checked;
}

QString CheckboxSettingRow::defaultValueDisplayText() const
{
    return FamSettings().valueOrDefault(key_).toBool() ? tr("Checked")
                                                       : tr("Unchecked");
}

void CheckboxSettingRow::setControlEnabled(bool enabled)
{
    input_->setEnabled(enabled);
}

// ─── IntegerSettingRow ──────────────────────────────────────────────────────

IntegerSettingRow::IntegerSettingRow(
    const QString& label, const QString& key, int min, int max, QWidget* parent)
    : SettingRowBase(label, key, parent)
    , input_([&] {
        const auto& sp = familiar::settings_style::palette();
        return new FlatSpinBox(sp.chipBackground,
                               sp.text,
                               sp.chipBackground.darker(112),
                               this);
    }())
{
    FamSettings settings;
    input_->setRange(min, max);
    input_->setFixedWidth(kControlWidth);
    setValue(settings.valueOrDefault(key_));
    hbox_->addWidget(input_);
    ignoreValueChanged_ = false;

    connect(input_, &QSpinBox::valueChanged, this, [this](int v) {
        onValueChanged(v);
    });

    refreshInfoPopup();
}

void IntegerSettingRow::setValue(const QVariant& value)
{
    input_->setValue(value.toInt());
}

void IntegerSettingRow::setControlEnabled(bool enabled)
{
    input_->setEnabled(enabled);
}

// ─── Concrete Performance-page rows ─────────────────────────────────────────

UndoHistorySizeRow::UndoHistorySizeRow(QWidget* parent)
    : IntegerSettingRow(QStringLiteral("Undo History Size"),
                        QStringLiteral("Items/undo_history_size"),
                        0,
                        10000,
                        parent)
{}

AutoOptimizeImportedImagesRow::AutoOptimizeImportedImagesRow(QWidget* parent)
    : ComboSettingRow(QStringLiteral("Auto Optimize Imported Images"),
                      QStringLiteral("Items/auto_optimize_imported_images"),
                      {
                          {QStringLiteral("off"), QStringLiteral("Off")},
                          {QStringLiteral("warn"),
                           QStringLiteral("Large image warning")},
                          {QStringLiteral("optimize_large"),
                           QStringLiteral("Optimize large images")},
                      },
                      parent)
{}

AutosaveEnabledRow::AutosaveEnabledRow(QWidget* parent)
    : CheckboxSettingRow(QStringLiteral("Enable Autosave"),
                         QStringLiteral("Save/autosave_enabled"),
                         parent)
{}

AutosaveIntervalRow::AutosaveIntervalRow(QWidget* parent)
    : IntegerSettingRow(QStringLiteral("Autosave Interval (seconds)"),
                        QStringLiteral("Save/autosave_interval_seconds"),
                        1,
                        3600,
                        parent)
{}

// ─── Concrete Images & Items-page rows ──────────────────────────────────────

ArrangeGapRow::ArrangeGapRow(QWidget* parent)
    : IntegerSettingRow(QStringLiteral("Arrange Gap"),
                        QStringLiteral("Items/arrange_gap"),
                        0,
                        200,
                        parent)
{}

MaximumImageSizeRow::MaximumImageSizeRow(QWidget* parent)
    : IntegerSettingRow(QStringLiteral("Maximum Image Size (MB)"),
                        QStringLiteral("Items/image_allocation_limit"),
                        0,
                        1024,
                        parent)
{}

ArrangeDefaultRow::ArrangeDefaultRow(QWidget* parent)
    : ComboSettingRow(QStringLiteral("Default Arrange Method"),
                      QStringLiteral("Items/arrange_default"),
                      {
                          {QStringLiteral("optimal"), QStringLiteral("Optimal")},
                          {QStringLiteral("horizontal"),
                           QStringLiteral("Horizontal (by filename)")},
                          {QStringLiteral("vertical"),
                           QStringLiteral("Vertical (by filename)")},
                          {QStringLiteral("square"),
                           QStringLiteral("Square (by filename)")},
                      },
                      parent)
{}

#include "setting_row.h"

#include "core/settings.h"
#include <widgets/flat_checkbox.h>
#include <widgets/flat_combobox.h>
#include <widgets/flat_spinbox.h>
#include <widgets/settings_style.h>

namespace {
constexpr char CHANGED_SYMBOL[] = "✎";

constexpr char kPlaceholderInfo[] =
    "Description coming soon.";

constexpr int kControlWidth = 200;
} // namespace

// ─── HoverInfoLabel ─────────────────────────────────────────────────────────

HoverInfoLabel::HoverInfoLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent)
{
    // No native hand-cursor for "this has a tooltip" in Qt - WhatsThis
    // is the closest stock cursor that reads as "hover for more info".
    setCursor(Qt::WhatsThisCursor);
}

void HoverInfoLabel::setInfoText(const QString& html)
{
    setToolTip(html);
}

// ─── SettingRowBase ─────────────────────────────────────────────────────────

SettingRowBase::SettingRowBase(const QString& label,
                               const QString& infoText,
                               const QString& key,
                               QWidget* parent)
    : QWidget(parent)
    , key_(key)
    , baseLabel_(label)
    , hbox_(new QHBoxLayout(this))
    , label_(new HoverInfoLabel(label, this))
{
    // Searchable by the search box's applyGroupFilter() (ui/
    // settings_window.cpp) the same way SettingsGroupBase's title does
    // (settings_dialog.cpp) - raw label, no "✎" changed-marker.
    setObjectName(label);
    hbox_->setContentsMargins(0, 4, 0, 4);
    label_->setInfoText(infoText);
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
    if (settings.valueChanged(key_))
        text += QStringLiteral(" ") + QString::fromUtf8(CHANGED_SYMBOL);
    label_->setText(text);
}

void SettingRowBase::onValueChanged(const QVariant& value)
{
    if (ignoreValueChanged_)
        return;

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

// ─── ComboSettingRow ────────────────────────────────────────────────────────

ComboSettingRow::ComboSettingRow(const QString& label,
                                 const QString& infoText,
                                 const QString& key,
                                 const QList<ComboOption>& options,
                                 QWidget* parent)
    : SettingRowBase(label, infoText, key, parent)
    , input_([&] {
          const auto& sp = familiar::settings_style::palette();
          return new FlatComboBox(sp.chipBackground,
                                  sp.text,
                                  sp.chipBackground.darker(112),
                                  sp.mutedText,
                                  this);
      }())
    , options_(options)
{
    FamSettings settings;
    for (const ComboOption& opt : options_)
        input_->addItem(opt.label);
    input_->setFixedWidth(kControlWidth);
    // setValue() (via setCurrentIndex) happens before this connect(), so
    // it can't fire onValueChanged() with nothing listening yet - same
    // ordering trick settings_dialog.cpp's IntegerGroupWidget etc. use,
    // no ignoreValueChanged_ guard needed.
    setValue(settings.valueOrDefault(key_));
    hbox_->addWidget(input_);
    ignoreValueChanged_ = false;

    connect(input_,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (index >= 0 && index < options_.size())
                    onValueChanged(options_[index].value);
            });
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

// ─── CheckboxSettingRow ─────────────────────────────────────────────────────

CheckboxSettingRow::CheckboxSettingRow(const QString& label,
                                       const QString& infoText,
                                       const QString& key,
                                       QWidget* parent)
    : SettingRowBase(label, infoText, key, parent)
    , input_([&] {
          // FlatCheckBox (widgets/flat_checkbox.h) - hand-painted, not a
          // plain QCheckBox (QSS on a QCheckBox with no ::indicator rule
          // kills native indicator rendering outright). Deliberately
          // NOT settings_style::palette().border for the unchecked
          // outline either: FlatCheckBox halves whatever color it's
          // given again internally for the unchecked state, and
          // border's already-light #D8D8D8 at that combined alpha was
          // all but invisible on the white page (Max, by screenshot -
          // "сейчас сливается"). text survives that second alpha cut
          // and actually reads as a checkbox. No label text of its own
          // - the row's own HoverInfoLabel is the only label. Checked
          // fill is navSelectedBg (gray), not accent (orange) - Max:
          // the orange read as an error/warning color here, wanted the
          // same neutral gray the rest of this window's "on" states use
          // (sidebar's selected row, etc.) instead of an accent color.
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
}

void CheckboxSettingRow::setValue(const QVariant& value)
{
    input_->setChecked(value.toBool());
}

QVariant CheckboxSettingRow::convertValueFromQt(const QVariant& value)
{
    return value.value<Qt::CheckState>() == Qt::Checked;
}

// ─── IntegerSettingRow ──────────────────────────────────────────────────────

IntegerSettingRow::IntegerSettingRow(const QString& label,
                                     const QString& infoText,
                                     const QString& key,
                                     int min,
                                     int max,
                                     QWidget* parent)
    : SettingRowBase(label, infoText, key, parent)
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
}

void IntegerSettingRow::setValue(const QVariant& value)
{
    input_->setValue(value.toInt());
}

// ─── Concrete Performance-page rows ─────────────────────────────────────────

UndoHistorySizeRow::UndoHistorySizeRow(QWidget* parent)
    : IntegerSettingRow(QStringLiteral("Undo History Size"),
                        QString::fromUtf8(kPlaceholderInfo),
                        QStringLiteral("Items/undo_history_size"),
                        0,
                        10000,
                        parent)
{}

AutoOptimizeImportedImagesRow::AutoOptimizeImportedImagesRow(QWidget* parent)
    : ComboSettingRow(
          QStringLiteral("Auto Optimize Imported Images"),
          QString::fromUtf8(kPlaceholderInfo),
          QStringLiteral("Items/auto_optimize_imported_images"),
          {
              {QStringLiteral("off"), QStringLiteral("Off")},
              {QStringLiteral("warn"), QStringLiteral("Large image warning")},
              {QStringLiteral("optimize_large"),
               QStringLiteral("Optimize large images")},
              {QStringLiteral("optimize_all"),
               QStringLiteral("Optimize all images")},
          },
          parent)
{}

AutosaveEnabledRow::AutosaveEnabledRow(QWidget* parent)
    : CheckboxSettingRow(QStringLiteral("Autosave"),
                        QString::fromUtf8(kPlaceholderInfo),
                        QStringLiteral("Save/autosave_enabled"),
                        parent)
{}

AutosaveIntervalRow::AutosaveIntervalRow(QWidget* parent)
    : IntegerSettingRow(QStringLiteral("Autosave Interval (seconds)"),
                       QString::fromUtf8(kPlaceholderInfo),
                       QStringLiteral("Save/autosave_interval_seconds"),
                       1,
                       3600,
                       parent)
{}

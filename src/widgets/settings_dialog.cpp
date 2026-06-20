#include "settings_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>

#include "core/settings.h"

static constexpr char CHANGED_SYMBOL[] = "✎";

// ─── SettingsGroupBase ────────────────────────────────────────────────────────

SettingsGroupBase::SettingsGroupBase(const QString& title,
                                     const QString& helptext,
                                     const QString& key,
                                     QWidget* parent)
    : QGroupBox(parent)
    , key_(key)
    , vbox_(new QVBoxLayout(this))
{
    setLayout(vbox_);
    updateTitle();

    connect(&SettingsEvents::instance(), &SettingsEvents::restoreDefaults,
            this, &SettingsGroupBase::onRestoreDefaults);

    if (!helptext.isEmpty()) {
        auto* label = new QLabel(helptext, this);
        label->setWordWrap(true);
        vbox_->addWidget(label);
    }

    Q_UNUSED(title); // stored via updateTitle
    // Store raw title for updateTitle; use the passed value directly on first call.
    // We override with the full title logic in updateTitle(), but QGroupBox needs
    // the base title. Store it as the object name so updateTitle can retrieve it.
    setObjectName(title);
    updateTitle();
}

void SettingsGroupBase::updateTitle()
{
    FamSettings settings;
    QString title = objectName();
    if (settings.valueChanged(key_))
        title += QStringLiteral(" ") + QString::fromUtf8(CHANGED_SYMBOL);
    setTitle(title);
}

void SettingsGroupBase::onValueChanged(const QVariant& value)
{
    if (ignoreValueChanged_)
        return;

    FamSettings settings;
    const QVariant converted = convertValueFromQt(value);
    if (converted != settings.valueOrDefault(key_)) {
        settings.setValue(key_, converted);
        updateTitle();
    }
}

void SettingsGroupBase::onRestoreDefaults()
{
    FamSettings settings;
    ignoreValueChanged_ = true;
    setValue(settings.valueOrDefault(key_));
    ignoreValueChanged_ = false;
    updateTitle();
}

// ─── RadioGroupWidget ─────────────────────────────────────────────────────────

RadioGroupWidget::RadioGroupWidget(const QString& title,
                                   const QString& helptext,
                                   const QString& key,
                                   const QList<RadioOption>& options,
                                   QWidget* parent)
    : SettingsGroupBase(title, helptext, key, parent)
{
    FamSettings settings;
    const QVariant current = settings.valueOrDefault(key_);

    ignoreValueChanged_ = true;
    for (const RadioOption& opt : options) {
        auto* btn = new QRadioButton(opt.label, this);
        btn->setToolTip(opt.tooltip);
        if (opt.value == current.toString())
            btn->setChecked(true);
        buttons_.insert(opt.value, btn);
        vbox_->addWidget(btn);

        connect(btn, &QRadioButton::toggled, this, [this, opt](bool checked) {
            if (checked)
                onValueChanged(opt.value);
        });
    }
    ignoreValueChanged_ = false;
    vbox_->addStretch(100);
}

void RadioGroupWidget::setValue(const QVariant& value)
{
    const QString str = value.toString();
    for (auto it = buttons_.constBegin(); it != buttons_.constEnd(); ++it)
        it.value()->setChecked(it.key() == str);
}

// ─── IntegerGroupWidget ───────────────────────────────────────────────────────

IntegerGroupWidget::IntegerGroupWidget(const QString& title,
                                       const QString& helptext,
                                       const QString& key,
                                       int min,
                                       int max,
                                       QWidget* parent)
    : SettingsGroupBase(title, helptext, key, parent)
    , input_(new QSpinBox(this))
{
    FamSettings settings;
    input_->setRange(min, max);
    setValue(settings.valueOrDefault(key_));
    vbox_->addWidget(input_);
    vbox_->addStretch(100);
    ignoreValueChanged_ = false;

    connect(input_, &QSpinBox::valueChanged, this, [this](int v) {
        onValueChanged(v);
    });
}

void IntegerGroupWidget::setValue(const QVariant& value)
{
    input_->setValue(value.toInt());
}

// ─── SingleCheckboxGroupWidget ────────────────────────────────────────────────

SingleCheckboxGroupWidget::SingleCheckboxGroupWidget(const QString& title,
                                                     const QString& helptext,
                                                     const QString& key,
                                                     const QString& label,
                                                     QWidget* parent)
    : SettingsGroupBase(title, helptext, key, parent)
    , input_(new QCheckBox(label, this))
{
    FamSettings settings;
    setValue(settings.valueOrDefault(key_));
    vbox_->addWidget(input_);
    vbox_->addStretch(100);
    ignoreValueChanged_ = false;

    connect(input_, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        onValueChanged(QVariant::fromValue(state));
    });
}

void SingleCheckboxGroupWidget::setValue(const QVariant& value)
{
    input_->setChecked(value.toBool());
}

QVariant SingleCheckboxGroupWidget::convertValueFromQt(const QVariant& value)
{
    return value.value<Qt::CheckState>() == Qt::Checked;
}

// ─── Concrete setting widgets ─────────────────────────────────────────────────

ArrangeDefaultWidget::ArrangeDefaultWidget(QWidget* parent)
    : RadioGroupWidget(
          QStringLiteral("Default Arrange Method:"),
          QStringLiteral("How images are arranged when inserted in batch"),
          QStringLiteral("Items/arrange_default"),
          {
              {QStringLiteral("optimal"),    QStringLiteral("Optimal"),
               QStringLiteral("Arrange Optimal")},
              {QStringLiteral("horizontal"), QStringLiteral("Horizontal (by filename)"),
               QStringLiteral("Arrange Horizontal (by filename)")},
              {QStringLiteral("vertical"),   QStringLiteral("Vertical (by filename)"),
               QStringLiteral("Arrange Vertical (by filename)")},
              {QStringLiteral("square"),     QStringLiteral("Square (by filename)"),
               QStringLiteral("Arrange Square (by filename)")},
          },
          parent)
{}

ImageStorageFormatWidget::ImageStorageFormatWidget(QWidget* parent)
    : RadioGroupWidget(
          QStringLiteral("Image Storage Format:"),
          QStringLiteral("How images are stored inside bee files."
                         " Changes will only take effect on newly saved images."),
          QStringLiteral("Items/image_storage_format"),
          {
              {QStringLiteral("best"), QStringLiteral("Best Guess"),
               QStringLiteral("Small images and images with alpha channel are stored as png,"
                              " everything else as jpg")},
              {QStringLiteral("png"),  QStringLiteral("Always PNG"),
               QStringLiteral("Lossless, but large bee file")},
              {QStringLiteral("jpg"),  QStringLiteral("Always JPG"),
               QStringLiteral("Small bee file, but lossy and no transparency support")},
          },
          parent)
{}

ArrangeGapWidget::ArrangeGapWidget(QWidget* parent)
    : IntegerGroupWidget(
          QStringLiteral("Arrange Gap:"),
          QStringLiteral("The gap between images when using arrange actions."),
          QStringLiteral("Items/arrange_gap"),
          0, 200,
          parent)
{}

AllocationLimitWidget::AllocationLimitWidget(QWidget* parent)
    : IntegerGroupWidget(
          QStringLiteral("Maximum Image Size:"),
          QStringLiteral("The maximum image size that can be loaded (in megabytes)."
                         " Set to 0 for no limitation."),
          QStringLiteral("Items/image_allocation_limit"),
          0, 10000,
          parent)
{}

ConfirmCloseUnsavedWidget::ConfirmCloseUnsavedWidget(QWidget* parent)
    : SingleCheckboxGroupWidget(
          QStringLiteral("Confirm when closing an unsaved file:"),
          QStringLiteral("When about to close an unsaved file, should the app ask for confirmation?"),
          QStringLiteral("Save/confirm_close_unsaved"),
          QStringLiteral("Confirm when closing"),
          parent)
{}

// ─── SettingsDialog ───────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(qApp->applicationName() + QStringLiteral(" Settings"));

    auto* tabs = new QTabWidget(this);

    // Miscellaneous tab
    auto* misc = new QWidget;
    auto* miscLayout = new QGridLayout(misc);
    miscLayout->addWidget(new ConfirmCloseUnsavedWidget, 0, 0);
    tabs->addTab(misc, QStringLiteral("&Miscellaneous"));

    // Images & Items tab
    auto* items = new QWidget;
    auto* itemsLayout = new QGridLayout(items);
    itemsLayout->addWidget(new ImageStorageFormatWidget, 0, 0);
    itemsLayout->addWidget(new AllocationLimitWidget,    0, 1);
    itemsLayout->addWidget(new ArrangeGapWidget,         1, 0);
    itemsLayout->addWidget(new ArrangeDefaultWidget,     1, 1);
    tabs->addTab(items, QStringLiteral("&Images && Items"));

    auto* layout = new QVBoxLayout(this);
    setLayout(layout);
    layout->addWidget(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* resetBtn = new QPushButton(QStringLiteral("&Restore Defaults"), this);
    resetBtn->setAutoDefault(false);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaults);
    buttons->addButton(resetBtn, QDialogButtonBox::ActionRole);

    layout->addWidget(buttons);
}

void SettingsDialog::onRestoreDefaults()
{
    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("Restore defaults?"),
        QStringLiteral("Do you want to restore all settings to their default values?"));

    if (reply == QMessageBox::Yes)
        FamSettings().restoreDefaults();
}

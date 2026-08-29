#include "restore_defaults_dialog.h"
#include "dialog_style.h"
#include "flat_checkbox.h"

#include <core/settings.h>
#include <core/settingshandler.h>

#include <QCheckBox>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

using familiar::SettingsCategory;

namespace {
constexpr int kIconSize = 40;

// FlatCheckBox's paintEvent only ever draws checked/unchecked (see its
// own comment on why) - no third "some but not all children checked"
// glyph, so "All" just reflects "are all four currently checked" as a
// plain on/off, not a real tristate indicator. Good enough for what
// this dialog needs.
QString categoryLabel(SettingsCategory category)
{
    switch (category) {
    case SettingsCategory::Performance:
        return QObject::tr("Performance");
    case SettingsCategory::ImagesAndItems:
        return QObject::tr("Images & Items");
    case SettingsCategory::Colors:
        return QObject::tr("Colors");
    case SettingsCategory::KeyboardShortcuts:
        return QObject::tr("Keyboard Shortcuts");
    }
    return {};
}

const QList<SettingsCategory>& allCategories()
{
    static const QList<SettingsCategory> categories = {
        SettingsCategory::Performance,
        SettingsCategory::ImagesAndItems,
        SettingsCategory::Colors,
        SettingsCategory::KeyboardShortcuts,
    };
    return categories;
}

// JSON key SettingsHandler stores the current preset's overrides under
// - matches SettingsHandler::setDefaultCurrentPreset()'s own switch
// exactly (that's the removal side of this, this is just "does it
// exist at all").
QString currentPresetJsonKey()
{
    switch (SettingsHandler::getInstance()->currentPreset()) {
    case EPresets::kDarkPreset:
        return QStringLiteral("darkColorPreset");
    case EPresets::kLightPreset:
        return QStringLiteral("lightColorPreset");
    case EPresets::kCustom1:
        return QStringLiteral("customPreset1");
    case EPresets::kCustom2:
        return QStringLiteral("customPreset2");
    case EPresets::kCustom3:
        return QStringLiteral("customPreset3");
    }
    return {};
}

// Whether `category` currently has anything that would actually change
// if restored - used to pre-check its checkbox, so the dialog opens
// with only the categories that have real, saved customizations ticked
// instead of everything unchecked by default.
bool categoryHasChanges(SettingsCategory category)
{
    switch (category) {
    case SettingsCategory::Performance:
    case SettingsCategory::ImagesAndItems: {
        FamSettings settings;
        for (const QString& key :
             RestoreDefaultsDialog::famSettingsKeysFor(category)) {
            if (settings.valueChanged(key)) {
                return true;
            }
        }
        return false;
    }
    case SettingsCategory::Colors:
        // Current preset's own colors: presence-check is valid here -
        // setDefaultCurrentPreset() resets it via removeJsonValue(), so
        // the key is only ever there at all if it was actually customized.
        //
        // masterOpacity can't use that same presence check - it's ONE
        // JSON key shared across every preset (QMap<presetIndex,
        // opacity>), not a per-preset key like the color list above, so
        // it can't be removeJsonValue()'d on reset without wiping every
        // OTHER preset's opacity too (a real, deliberately rejected fix
        // - see setDefaultCurrentPreset()'s own comment). Its reset path
        // instead WRITES 255 back (setCurrentOpacity(255)), which means
        // the key stays present in storage forever after the very first
        // touch (a reset, or just dragging the slider) even once the
        // CURRENT preset's own value is genuinely back at its default -
        // a real bug this fixes (Colors came up pre-checked on every
        // single open of this dialog after that). Comparing the actual
        // value against its true default (opacityListDef's 255, same
        // for every preset) is the only check that works here.
        return !SettingsHandler::getInstance()
                    ->jsonValue(QStringLiteral("Colors"), currentPresetJsonKey())
                    .isUndefined()
            || SettingsHandler::getInstance()->getCurrentOpacity() != 255;
    case SettingsCategory::KeyboardShortcuts:
        // Can't use "is the Actions/Controls JSON group empty" the same
        // way Colors uses preset-key presence - KeyboardSettings::
        // saveUnknownShortcuts defaults to true (controls.h), so EVERY
        // action's shortcut gets persisted the moment it's registered,
        // whether or not it's actually different from its code default.
        // That made this checkbox come up checked on every single
        // launch, customized or not (real bug Max hit). A correct check
        // would mean re-walking every Action/MouseConfig/
        // MouseWheelConfig and comparing bindings() against
        // defaultBindings() (BindingTarget::bindingsChanged() already
        // does this per-target) - not worth replicating that
        // construction here just for a pre-check convenience feature,
        // so this category simply starts unchecked like the rest did
        // before this pre-check existed.
        return false;
    }
    return false;
}

} // namespace

QStringList RestoreDefaultsDialog::famSettingsKeysFor(SettingsCategory category)
{
    switch (category) {
    case SettingsCategory::Performance:
        // NOT "Save/confirm_close_unsaved" - that string only ever
        // appeared as a documentation example in settings.cpp's own
        // keyGroup()/keySubkey() comment, never as an actual
        // FamSettings::fields() entry. Passing it to valueChanged()/
        // valueOrDefault() hits Q_ASSERT(f.contains(key)) and aborts -
        // confirmed the hard way by a real crash the moment this
        // dialog opened.
        return {
            QStringLiteral("Save/autosave_enabled"),
            QStringLiteral("Save/autosave_interval_seconds"),
            QStringLiteral("Items/undo_history_size"),
            QStringLiteral("Items/auto_optimize_imported_images"),
        };
    case SettingsCategory::ImagesAndItems:
        return {
            QStringLiteral("Items/image_allocation_limit"),
            QStringLiteral("Items/arrange_gap"),
            QStringLiteral("Items/arrange_default"),
            QStringLiteral("Items/image_storage_format"),
        };
    case SettingsCategory::Colors:
    case SettingsCategory::KeyboardShortcuts:
        return {};
    }
    return {};
}

RestoreDefaultsDialog::RestoreDefaultsDialog(QWidget* parent)
    : QDialog(parent)
{
    // Deliberately NOT WA_DeleteOnClose - this dialog is used modally
    // (exec()), and Qt's own docs warn that combination can free the
    // object before exec() even returns (its nested event loop can
    // process the deleteLater() before unwinding) - confirmed the hard
    // way: a real use-after-free crash in checkedCategories() right
    // after exec() returned Accepted. settings_window.cpp's own caller
    // now stack-allocates this instead of `new`ing it, so there's
    // nothing to manually delete either.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Restore Defaults"));
    setMinimumWidth(340);
    setMaximumWidth(420);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];
    // Fixed red, not the theme accent - flags this specific button as
    // destructive regardless of which color preset is active (matches
    // the reference screenshot Max sent).
    const QColor destructive(211, 47, 47);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 16, 16, 16);
    outer->setSpacing(14);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(kIconSize, kIconSize);
    iconLabel->setPixmap(
        familiar::dialog_style::severityIcon(QMessageBox::Warning,
                                             accent,
                                             devicePixelRatioF()));
    topRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(4);
    auto* titleLabel = new QLabel(tr("Restore Defaults"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    textCol->addWidget(titleLabel);

    auto* textLabel = new QLabel(
        tr("This action can't be undone, consider exporting a backup "
           "first."),
        this);
    textLabel->setWordWrap(true);
    textCol->addWidget(textLabel);
    topRow->addLayout(textCol, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("rddCloseBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &RestoreDefaultsDialog::reject);
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addLayout(topRow);

    auto* listLayout = new QVBoxLayout();
    listLayout->setSpacing(6);

    allCheckbox_ = new FlatCheckBox(tr("All"), textColor, border, accent, this);
    allCheckbox_->setChecked(false);
    // checkStateChanged(Qt::CheckState) is Qt 6.9+ only, stateChanged(int)
    // is deprecated there but still present - see widgets/setting_row.cpp's
    // own comment on the identical split for why this is an #ifdef and
    // not just one or the other.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    connect(allCheckbox_, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState state) {
                onAllToggled_(static_cast<int>(state));
            });
#else
    connect(allCheckbox_, &QCheckBox::stateChanged, this,
            [this](int state) { onAllToggled_(state); });
#endif
    listLayout->addWidget(allCheckbox_);

    for (SettingsCategory category : allCategories()) {
        auto* checkbox = new FlatCheckBox(categoryLabel(category),
                                          textColor,
                                          border,
                                          accent,
                                          this);
        // Pre-check only categories that actually have a saved
        // customization, so opening the dialog highlights what's
        // actually changeable instead of starting fully unchecked.
        checkbox->setChecked(categoryHasChanges(category));
        // Same Qt 6.9 checkStateChanged/stateChanged split as
        // allCheckbox_ above - the value itself isn't needed here
        // either way, just that *something* changed.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        connect(checkbox, &QCheckBox::checkStateChanged, this,
                [this](Qt::CheckState) { onCategoryToggled_(); });
#else
        connect(checkbox, &QCheckBox::stateChanged, this,
                [this](int) { onCategoryToggled_(); });
#endif
        categoryCheckboxes_.insert(category, checkbox);

        auto* row = new QHBoxLayout();
        row->setContentsMargins(20, 0, 0, 0);
        row->addWidget(checkbox);
        listLayout->addLayout(row);
    }
    // Reflects "All" against whatever the pre-check loop above just
    // set, in case every category happened to have changes already -
    // same logic a later manual toggle would use, just run once here.
    onCategoryToggled_();

    outer->addLayout(listLayout);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    familiar::dialog_style::styleSecondaryButton(cancelBtn, textColor, border);
    connect(cancelBtn, &QPushButton::clicked, this, &RestoreDefaultsDialog::reject);
    buttonRow->addWidget(cancelBtn);

    auto* restoreBtn = new QPushButton(tr("Restore to Default"), this);
    familiar::dialog_style::stylePrimaryButton(restoreBtn, destructive);
    connect(restoreBtn, &QPushButton::clicked, this, &RestoreDefaultsDialog::accept);
    buttonRow->addWidget(restoreBtn);

    outer->addLayout(buttonRow);

    setStyleSheet(
        familiar::dialog_style::panelStyleSheet("RestoreDefaultsDialog",
                                                background,
                                                border,
                                                textColor,
                                                /*radiusPx=*/0)
        + familiar::dialog_style::closeButtonStyleSheet("rddCloseBtn",
                                                        textColor,
                                                        accent));
}

QList<SettingsCategory> RestoreDefaultsDialog::checkedCategories() const
{
    QList<SettingsCategory> result;
    for (auto it = categoryCheckboxes_.constBegin();
         it != categoryCheckboxes_.constEnd();
         ++it) {
        if (it.value()->isChecked()) {
            result.append(it.key());
        }
    }
    return result;
}

void RestoreDefaultsDialog::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so this IS the title bar for drag purposes - same
    // idiom every other custom top-level dialog in this app uses.
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void RestoreDefaultsDialog::onAllToggled_(int state)
{
    if (syncing_) {
        return;
    }
    syncing_ = true;
    const bool checked = (state == Qt::Checked);
    for (QCheckBox* checkbox : std::as_const(categoryCheckboxes_)) {
        checkbox->setChecked(checked);
    }
    syncing_ = false;
}

void RestoreDefaultsDialog::onCategoryToggled_()
{
    if (syncing_) {
        return;
    }
    syncing_ = true;
    bool allChecked = true;
    for (QCheckBox* checkbox : std::as_const(categoryCheckboxes_)) {
        if (!checkbox->isChecked()) {
            allChecked = false;
            break;
        }
    }
    allCheckbox_->setChecked(allChecked);
    syncing_ = false;
}

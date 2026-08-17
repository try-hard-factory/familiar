#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QSpinBox>
#include <QString>
#include <QWidget>

// Rich hover popup (widgets/setting_row.cpp): bold title +
// body copy, an optional muted "Default: X" line, and an optional
// "Ctrl + Dbl click to restore default" footer hint - opaque white panel,
// square corners (NOT dialog_style::panelStyleSheet()/applyRoundedMask() -
// same square-window-under-rounded-QSS-paint artifact SettingsWindow's
// own history already ran into, see this class's .cpp for the full
// comment), NOT WA_TranslucentBackground either (see dialog_style.h for
// why that combo is off the table on a top-level widget in this
// codebase). Qt::ToolTip window flag: never steals focus/keyboard, no
// taskbar entry, and WA_TransparentForMouseEvents so it never blocks the
// row underneath it (e.g. the Ctrl+double-click gesture the footer hint
// describes still has to land on the row, not on this popup covering it).
class SettingInfoPopup : public QWidget
{
    Q_OBJECT
public:
    explicit SettingInfoPopup(QWidget* parent = nullptr);

    // `defaultText`/`showResetHint` both off hides those two lines
    // entirely - used by a plain (non-SettingRowBase) HoverInfoLabel,
    // e.g. Keyboard Shortcuts' rows, which have no backing settings key
    // for either concept.
    void setContent(const QString& title,
                    const QString& bodyHtml,
                    const QString& defaultText,
                    bool showResetHint);

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    QLabel* defaultLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
};

// A settings-row label that shows an info tooltip on hover - this app's
// own Performance/Images & Items pages drop the always-visible help
// paragraph the old QGroupBox-based settings widgets (settings_dialog.h,
// removed once every page had migrated to this row shape) printed under
// every group box, in favor of a flat list of rows whose description
// only shows up on hover. The popup is SettingInfoPopup above (custom),
// not native QToolTip - setToolTip() is still called
// (setInfoText() below) so the text survives for accessibility/anything
// else that reads QWidget::toolTip(), but QEvent::ToolTip is intercepted
// (see event() override) to show SettingInfoPopup instead of letting Qt
// render it natively. The body text itself isn't a constructor
// parameter anywhere in this file any more - SettingRowBase looks it up
// from widgets/setting_descriptions.h by settings key, the one place all
// of it lives now (some entries are still a shared placeholder there -
// real copy for those is a follow-up).
class HoverInfoLabel : public QLabel
{
    Q_OBJECT
public:
    explicit HoverInfoLabel(const QString& text, QWidget* parent = nullptr);

    void setInfoText(const QString& html);
    // "Default: X" popup line - empty (the default) hides it.
    void setDefaultText(const QString& text);
    // "Ctrl + Dbl click to restore default" popup footer hint - off by
    // default. Only SettingRowBase turns this on (see
    // SettingRowBase::refreshInfoPopup()) - the gesture doesn't mean
    // anything without a backing settings key.
    void setShowResetHint(bool show);

protected:
    bool event(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString title_;
    QString infoHtml_;
    QString defaultText_;
    bool showResetHint_ = false;
    SettingInfoPopup* popup_ = nullptr; // lazily created, parented to window()
};

// ─── SettingRowBase ─────────────────────────────────────────────────────────

// Row-shaped counterpart to the old QGroupBox-based SettingsGroupBase
// (settings_dialog.h, removed): same persistence / restore-defaults /
// changed-indicator ("✎") plumbing, but a flat "label ... control"
// QHBoxLayout instead of a QGroupBox with a title and an always-visible
// helptext paragraph.
class SettingRowBase : public QWidget
{
    Q_OBJECT
public:
    // Info-popup body text is looked up internally from `key` via
    // widgets/setting_descriptions.h - not a constructor parameter, so
    // there's exactly one place (that file) to read/edit every row's
    // copy instead of a literal at each of this class's ~10 call sites.
    explicit SettingRowBase(const QString& label,
                            const QString& key,
                            QWidget* parent = nullptr);

    // Disables just the input control - NOT QWidget::setEnabled() on the
    // whole row, which would take label_ down with it. Qt doesn't
    // dispatch hover/enter events (and so never applies a custom
    // setCursor()) to a disabled widget, while still special-casing
    // tooltips to work on one - so a fully-disabled row ends up with its
    // tooltip working but its WhatsThisCursor hand cursor silently
    // stuck at the default arrow. Used for a row that only matters
    // alongside a sibling (e.g. AutosaveIntervalRow while
    // AutosaveEnabledRow is unchecked) - the label should stay
    // explorable either way.
    virtual void setControlEnabled(bool enabled) = 0;

protected:
    virtual void setValue(const QVariant& value) = 0;
    virtual QVariant convertValueFromQt(const QVariant& value) { return value; }

    // Human-readable default value for the info popup's "Default: X"
    // line. Base impl is fine for a plain scalar (IntegerSettingRow);
    // ComboSettingRow/CheckboxSettingRow override it to show the
    // option's label / Checked-Unchecked instead of the raw stored
    // value ("warn", "true", ...). Each concrete row shape's own
    // constructor calls refreshInfoPopup() (below) at the very end of
    // its body, once whatever state this override reads (e.g.
    // ComboSettingRow::options_) is fully constructed - NOT from
    // SettingRowBase's own constructor, where virtual dispatch would
    // still resolve to this base version regardless of the real type
    // under construction.
    virtual QString defaultValueDisplayText() const;

    // Feeds label_'s popup its "Default: X" line + turns on the
    // "Ctrl + Dbl click to restore default" footer hint - see
    // defaultValueDisplayText()'s comment for why each concrete row
    // shape's constructor must call this itself, at the end of its body.
    void refreshInfoPopup();

    void updateLabel();
    void onValueChanged(const QVariant& value);

    // Ctrl+double-click anywhere on the row (not already consumed by a
    // child widget, e.g. inside the spinbox's own line edit) resets just
    // this one field to default - the gesture SettingInfoPopup's footer
    // hint (refreshInfoPopup() above) describes. Unlike
    // onRestoreDefaults() below (driven by the page-wide "Restore
    // Defaults" button, which has already cleared the whole JSON group
    // itself before emitting SettingsEvents::restoreDefaults()), this
    // has to remove the stored override for key_ itself.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    QString key_;
    QString baseLabel_;
    QHBoxLayout* hbox_ = nullptr;
    HoverInfoLabel* label_ = nullptr;
    bool ignoreValueChanged_ = false;

private slots:
    void onRestoreDefaults();
};

// ─── Concrete row shapes ────────────────────────────────────────────────────

struct ComboOption
{
    QString value;
    QString label;
};

class ComboSettingRow : public SettingRowBase
{
    Q_OBJECT
public:
    explicit ComboSettingRow(const QString& label,
                             const QString& key,
                             const QList<ComboOption>& options,
                             QWidget* parent = nullptr);

    void setControlEnabled(bool enabled) override;

protected:
    void setValue(const QVariant& value) override;
    QString defaultValueDisplayText() const override;

private:
    QComboBox* input_ = nullptr;
    QList<ComboOption> options_;
};

class CheckboxSettingRow : public SettingRowBase
{
    Q_OBJECT
public:
    explicit CheckboxSettingRow(const QString& label,
                                const QString& key,
                                QWidget* parent = nullptr);

    void setControlEnabled(bool enabled) override;

signals:
    // Same purpose as SingleCheckboxGroupWidget::toggled() used to serve
    // - wiring a dependent row's setControlEnabled() live
    // (AutosaveIntervalRow under AutosaveEnabledRow in
    // ui/settings_window.cpp).
    void toggled(bool checked);

protected:
    void setValue(const QVariant& value) override;
    QVariant convertValueFromQt(const QVariant& value) override;
    QString defaultValueDisplayText() const override;

private:
    QCheckBox* input_ = nullptr; // actually a FlatCheckBox, see .cpp
};

class IntegerSettingRow : public SettingRowBase
{
    Q_OBJECT
public:
    explicit IntegerSettingRow(const QString& label,
                               const QString& key,
                               int min,
                               int max,
                               QWidget* parent = nullptr);

    void setControlEnabled(bool enabled) override;

protected:
    void setValue(const QVariant& value) override;

private:
    QSpinBox* input_ = nullptr;
};

// ─── Concrete Performance-page rows ─────────────────────────────────────────

class UndoHistorySizeRow : public IntegerSettingRow
{
public:
    explicit UndoHistorySizeRow(QWidget* parent = nullptr);
};

class AutoOptimizeImportedImagesRow : public ComboSettingRow
{
public:
    explicit AutoOptimizeImportedImagesRow(QWidget* parent = nullptr);
};

class AutosaveEnabledRow : public CheckboxSettingRow
{
public:
    explicit AutosaveEnabledRow(QWidget* parent = nullptr);
};

class AutosaveIntervalRow : public IntegerSettingRow
{
public:
    explicit AutosaveIntervalRow(QWidget* parent = nullptr);
};

// ─── Concrete Images & Items-page rows ──────────────────────────────────────
// Replaces settings_dialog.h's SettingsGroupBase-derived
// ArrangeGapWidget/AllocationLimitWidget/ArrangeDefaultWidget (and drops
// ImageStorageFormatWidget outright - UI removed, the underlying
// Items/image_storage_format setting/facade/get_imgformat() usage is
// untouched) - same flat-row shape as the Performance page above.

class ArrangeGapRow : public IntegerSettingRow
{
public:
    explicit ArrangeGapRow(QWidget* parent = nullptr);
};

class MaximumImageSizeRow : public IntegerSettingRow
{
public:
    explicit MaximumImageSizeRow(QWidget* parent = nullptr);
};

class ArrangeDefaultRow : public ComboSettingRow
{
public:
    explicit ArrangeDefaultRow(QWidget* parent = nullptr);
};

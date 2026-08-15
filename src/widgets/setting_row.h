#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QSpinBox>
#include <QString>
#include <QWidget>

// A settings-row label that shows an info tooltip on hover - PureRef's
// own Performance page drops the always-visible help paragraph
// SettingsGroupBase (settings_dialog.h) prints under every group box, in
// favor of a flat list of rows whose description only shows up on
// hover. Native QToolTip for now (supports basic rich text, shows on
// hover for free) - structure only, per Max: every row's info text
// below is a placeholder, real per-setting copy is a follow-up.
class HoverInfoLabel : public QLabel
{
    Q_OBJECT
public:
    explicit HoverInfoLabel(const QString& text, QWidget* parent = nullptr);

    void setInfoText(const QString& html);
};

// ─── SettingRowBase ─────────────────────────────────────────────────────────

// Row-shaped counterpart to SettingsGroupBase (settings_dialog.h): same
// persistence / restore-defaults / changed-indicator ("✎") plumbing, but
// a flat "label ... control" QHBoxLayout instead of a QGroupBox with a
// title and an always-visible helptext paragraph.
class SettingRowBase : public QWidget
{
    Q_OBJECT
public:
    explicit SettingRowBase(const QString& label,
                            const QString& infoText,
                            const QString& key,
                            QWidget* parent = nullptr);

protected:
    virtual void setValue(const QVariant& value) = 0;
    virtual QVariant convertValueFromQt(const QVariant& value) { return value; }

    void updateLabel();
    void onValueChanged(const QVariant& value);

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
                             const QString& infoText,
                             const QString& key,
                             const QList<ComboOption>& options,
                             QWidget* parent = nullptr);

protected:
    void setValue(const QVariant& value) override;

private:
    QComboBox* input_ = nullptr;
    QList<ComboOption> options_;
};

class CheckboxSettingRow : public SettingRowBase
{
    Q_OBJECT
public:
    explicit CheckboxSettingRow(const QString& label,
                                const QString& infoText,
                                const QString& key,
                                QWidget* parent = nullptr);

signals:
    // Same purpose as SingleCheckboxGroupWidget::toggled() used to serve
    // - wiring a dependent row's setEnabled() live (AutosaveIntervalRow
    // under AutosaveEnabledRow in ui/settings_window.cpp).
    void toggled(bool checked);

protected:
    void setValue(const QVariant& value) override;
    QVariant convertValueFromQt(const QVariant& value) override;

private:
    QCheckBox* input_ = nullptr; // actually a FlatCheckBox, see .cpp
};

class IntegerSettingRow : public SettingRowBase
{
    Q_OBJECT
public:
    explicit IntegerSettingRow(const QString& label,
                               const QString& infoText,
                               const QString& key,
                               int min,
                               int max,
                               QWidget* parent = nullptr);

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

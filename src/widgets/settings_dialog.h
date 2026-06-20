#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QMap>

// ─── SettingsGroupBase ────────────────────────────────────────────────────────

class SettingsGroupBase : public QGroupBox
{
    Q_OBJECT
public:
    explicit SettingsGroupBase(const QString& title,
                               const QString& helptext,
                               const QString& key,
                               QWidget* parent = nullptr);

protected:
    virtual void setValue(const QVariant& value) = 0;
    virtual QVariant convertValueFromQt(const QVariant& value) { return value; }

    void updateTitle();
    void onValueChanged(const QVariant& value);

    QString key_;
    QVBoxLayout* vbox_ = nullptr;
    bool ignoreValueChanged_ = false;

private slots:
    void onRestoreDefaults();
};

// ─── RadioGroupWidget ─────────────────────────────────────────────────────────

struct RadioOption {
    QString value;
    QString label;
    QString tooltip;
};

class RadioGroupWidget : public SettingsGroupBase
{
    Q_OBJECT
public:
    explicit RadioGroupWidget(const QString& title,
                              const QString& helptext,
                              const QString& key,
                              const QList<RadioOption>& options,
                              QWidget* parent = nullptr);

protected:
    void setValue(const QVariant& value) override;

private:
    QMap<QString, QRadioButton*> buttons_;
};

// ─── IntegerGroupWidget ───────────────────────────────────────────────────────

class IntegerGroupWidget : public SettingsGroupBase
{
    Q_OBJECT
public:
    explicit IntegerGroupWidget(const QString& title,
                                const QString& helptext,
                                const QString& key,
                                int min,
                                int max,
                                QWidget* parent = nullptr);

protected:
    void setValue(const QVariant& value) override;

private:
    QSpinBox* input_ = nullptr;
};

// ─── SingleCheckboxGroupWidget ────────────────────────────────────────────────

class SingleCheckboxGroupWidget : public SettingsGroupBase
{
    Q_OBJECT
public:
    explicit SingleCheckboxGroupWidget(const QString& title,
                                       const QString& helptext,
                                       const QString& key,
                                       const QString& label,
                                       QWidget* parent = nullptr);

protected:
    void setValue(const QVariant& value) override;
    QVariant convertValueFromQt(const QVariant& value) override;

private:
    QCheckBox* input_ = nullptr;
};

// ─── Concrete setting widgets ─────────────────────────────────────────────────

class ArrangeDefaultWidget : public RadioGroupWidget
{
public:
    explicit ArrangeDefaultWidget(QWidget* parent = nullptr);
};

class ImageStorageFormatWidget : public RadioGroupWidget
{
public:
    explicit ImageStorageFormatWidget(QWidget* parent = nullptr);
};

class ArrangeGapWidget : public IntegerGroupWidget
{
public:
    explicit ArrangeGapWidget(QWidget* parent = nullptr);
};

class AllocationLimitWidget : public IntegerGroupWidget
{
public:
    explicit AllocationLimitWidget(QWidget* parent = nullptr);
};

class ConfirmCloseUnsavedWidget : public SingleCheckboxGroupWidget
{
public:
    explicit ConfirmCloseUnsavedWidget(QWidget* parent = nullptr);
};

// ─── SettingsDialog ───────────────────────────────────────────────────────────

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onRestoreDefaults();
};

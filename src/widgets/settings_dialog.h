#pragma once

#include <QGroupBox>
#include <QMap>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

// ─── SettingsGroupBase ────────────────────────────────────────────────────────

class SettingsGroupBase : public QGroupBox
{
    Q_OBJECT
public:
    explicit SettingsGroupBase(const QString& title,
                               const QString& helptext,
                               const QString& key,
                               QWidget* parent = nullptr);

    // Inserts `w` into this group's own layout, just before the trailing
    // stretch - for a setting that's only meaningful alongside this one
    // (e.g. an interval spinbox that does nothing unless a sibling
    // checkbox enables it) and should read as visually nested under it
    // rather than as a separate, unrelated group box.
    void addNestedWidget(QWidget* w)
    {
        vbox_->insertWidget(vbox_->count() - 1, w);
    }

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

struct RadioOption
{
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


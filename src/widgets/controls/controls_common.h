#pragma once

#include <QAbstractTableModel>
#include <QCheckBox>
#include <QDialog>
#include <QList>
#include <QMap>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>

// ─── MouseControlsEditorBase ──────────────────────────────────────────────────

class MouseControlsEditorBase : public QDialog
{
    Q_OBJECT

public:
    int conflictingRow() const { return conflictingRow_; }

signals:
    void saved();

protected:
    explicit MouseControlsEditorBase(QWidget* parent = nullptr);

    void initModifiersInput();
    void initButtonRow();
    void setModifiersNoModifier();

    virtual QStringList getModifiers(bool cleaned = true) const;
    void setModifiers(const QStringList& modifiers);

    // Subclasses return the row index of the conflicting action, or -1 if none.
    virtual int findConflictingRow() const = 0;
    // Subclasses return a display string for the conflicting action at the given row.
    virtual QString conflictingActionText(int row) const = 0;
    // Subclasses reset all inputs to the stored values.
    virtual void resetInputs() = 0;

    QVBoxLayout* mainLayout_ = nullptr;
    QMap<QString, QCheckBox*> checkboxes_;
    bool ignoreOnChanged_ = false;
    int conflictingRow_ = -1;

private slots:
    void onModifiersChanged(const QString& modifier, int value);
    void onSave();
};

// ─── MouseControlsModelBase ───────────────────────────────────────────────────

class MouseControlsModelBase : public QAbstractTableModel
{
    Q_OBJECT

public:
    static constexpr int COL_ACTION    = 1;
    static constexpr int COL_CHANGED   = 2;
    static constexpr int COL_BUTTON    = 3;
    static constexpr int COL_MODIFIERS = 4;
    static constexpr int COL_INVERTED  = 5;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole) override;

    // Extended setData that also removes controls from another row.
    // Pass removeFromOtherRow = -1 to skip the removal step.
    bool setDataEx(const QModelIndex& index, const QVariant& value,
                   int role, int removeFromOtherRow);

protected:
    explicit MouseControlsModelBase(QList<int> columns, QObject* parent = nullptr);

    virtual int actionCount() const = 0;
    virtual QString actionText(int row) const = 0;
    virtual bool actionControlsChanged(int row) const = 0;
    virtual QString actionButton(int row) const { return {}; }
    virtual QStringList actionModifiers(int row) const = 0;
    virtual bool actionInverted(int row) const = 0;
    virtual bool actionConfigured(int row) const = 0;
    virtual bool actionInvertible(int row) const = 0;
    virtual void setDataOnAction(int row, const QVariant& value) = 0;
    virtual void setActionInverted(int row, bool inverted) = 0;
    virtual void removeActionControls(int row) = 0;
    virtual QString actionDefaultButton(int row) const { return {}; }
    virtual QStringList actionDefaultModifiers(int row) const = 0;
    virtual bool actionDefaultInverted(int row) const = 0;

private:
    QList<int> columns_;
};

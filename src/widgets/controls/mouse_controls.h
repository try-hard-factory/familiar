#pragma once

#include "controls_common.h"

#include <QComboBox>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>

class MouseConfig;

// ─── MouseControlsEditor ─────────────────────────────────────────────────────

class MouseControlsEditor : public MouseControlsEditorBase
{
    Q_OBJECT

public:
    explicit MouseControlsEditor(QWidget* parent, const QModelIndex& index);

    QString getButton() const;
    QStringList getModifiers(bool cleaned = true) const override;

protected:
    int findConflictingRow() const override;
    QString conflictingActionText(int row) const override;
    void resetInputs() override;

private slots:
    void onButtonChanged();

private:
    void setModifiersEnabled(bool enabled);
    void setButton(const QString& value);

    const MouseConfig* currentAction_ = nullptr;
    QString oldButton_;
    QStringList oldModifiers_;
    QComboBox* buttonInput_ = nullptr;
};

// ─── MouseDelegate ────────────────────────────────────────────────────────────

class MouseProxy;

class MouseDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MouseDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
};

// ─── MouseModel ───────────────────────────────────────────────────────────────

class MouseModel : public MouseControlsModelBase
{
    Q_OBJECT

public:
    explicit MouseModel(QObject* parent = nullptr);

protected:
    int actionCount() const override;
    QString actionText(int row) const override;
    bool actionControlsChanged(int row) const override;
    QString actionButton(int row) const override;
    QStringList actionModifiers(int row) const override;
    bool actionInverted(int row) const override;
    bool actionConfigured(int row) const override;
    bool actionInvertible(int row) const override;
    void setDataOnAction(int row, const QVariant& value) override;
    void setActionInverted(int row, bool inverted) override;
    void removeActionControls(int row) override;
    QString actionDefaultButton(int row) const override;
    QStringList actionDefaultModifiers(int row) const override;
    bool actionDefaultInverted(int row) const override;
};

// ─── MouseProxy ───────────────────────────────────────────────────────────────

class MouseProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit MouseProxy(QObject* parent = nullptr);

    bool setDataEx(const QModelIndex& proxyIndex, const QVariant& value,
                   int role, int removeFromOtherRow);
};

// ─── MouseView ────────────────────────────────────────────────────────────────

class MouseView : public QTableView
{
    Q_OBJECT

public:
    explicit MouseView(QWidget* parent = nullptr);

private slots:
    void onRestoreDefaults();
};

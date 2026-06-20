#pragma once

#include "controls_common.h"

#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>

class MouseWheelConfig;

// ─── MouseWheelModifiersEditor ────────────────────────────────────────────────

class MouseWheelModifiersEditor : public MouseControlsEditorBase
{
    Q_OBJECT

public:
    explicit MouseWheelModifiersEditor(QWidget* parent, const QModelIndex& index);

protected:
    int findConflictingRow() const override;
    QString conflictingActionText(int row) const override;
    void resetInputs() override;

private:
    const MouseWheelConfig* currentAction_ = nullptr;
    QStringList oldModifiers_;
};

// ─── MouseWheelDelegate ───────────────────────────────────────────────────────

class MouseWheelProxy;

class MouseWheelDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MouseWheelDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
};

// ─── MouseWheelModel ──────────────────────────────────────────────────────────

class MouseWheelModel : public MouseControlsModelBase
{
    Q_OBJECT

public:
    explicit MouseWheelModel(QObject* parent = nullptr);

protected:
    int actionCount() const override;
    QString actionText(int row) const override;
    bool actionControlsChanged(int row) const override;
    QStringList actionModifiers(int row) const override;
    bool actionInverted(int row) const override;
    bool actionConfigured(int row) const override;
    bool actionInvertible(int row) const override;
    void setDataOnAction(int row, const QVariant& value) override;
    void setActionInverted(int row, bool inverted) override;
    void removeActionControls(int row) override;
    QStringList actionDefaultModifiers(int row) const override;
    bool actionDefaultInverted(int row) const override;
};

// ─── MouseWheelProxy ──────────────────────────────────────────────────────────

class MouseWheelProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit MouseWheelProxy(QObject* parent = nullptr);

    bool setDataEx(const QModelIndex& proxyIndex, const QVariant& value,
                   int role, int removeFromOtherRow);
};

// ─── MouseWheelView ───────────────────────────────────────────────────────────

class MouseWheelView : public QTableView
{
    Q_OBJECT

public:
    explicit MouseWheelView(QWidget* parent = nullptr);

private slots:
    void onRestoreDefaults();
};

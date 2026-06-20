#pragma once

#include <QAbstractTableModel>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVariant>

struct Action;

// ─── KeyboardShortcutsEditor ──────────────────────────────────────────────────

class KeyboardShortcutsEditor : public QKeySequenceEdit
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsEditor(QWidget* parent, const QModelIndex& index);

    int conflictingRow() const { return conflictingRow_; }

private slots:
    void onEditingFinished();

private:
    Action* action_ = nullptr;
    QString oldValue_;
    QString lastCalledWith_;
    int conflictingRow_ = -1;
};

// ─── KeyboardShortcutsDelegate ────────────────────────────────────────────────

class KeyboardShortcutsProxy;

class KeyboardShortcutsDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
};

// ─── KeyboardShortcutsModel ───────────────────────────────────────────────────

class KeyboardShortcutsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    static inline QStringList HEADER = {
        QStringLiteral("Action"),
        QStringLiteral("✎"),
        QStringLiteral("Shortcut"),
        QStringLiteral("Alternative"),
    };

    explicit KeyboardShortcutsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole) override;

    // Extended version with explicit remove-from-other-row parameter.
    bool setDataEx(const QModelIndex& index, const QKeySequence& keySeq,
                   int role, int removeFromOtherRow);
};

// ─── KeyboardShortcutsProxy ───────────────────────────────────────────────────

class KeyboardShortcutsProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsProxy(QObject* parent = nullptr);

    // Forwarding method that passes removeFromOtherRow through to the source model.
    bool setDataEx(const QModelIndex& proxyIndex, const QKeySequence& keySeq,
                   int role, int removeFromOtherRow);
};

// ─── KeyboardShortcutsView ────────────────────────────────────────────────────

class KeyboardShortcutsView : public QTableView
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsView(QWidget* parent = nullptr);

private slots:
    void onRestoreDefaults();
};

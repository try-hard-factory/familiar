#include "keyboard_controls.h"

#include "actions/actions.h"
#include "core/controls.h"
#include "core/settings.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSet>

// ─── KeyboardShortcutsEditor ──────────────────────────────────────────────────

KeyboardShortcutsEditor::KeyboardShortcutsEditor(QWidget* parent,
                                                 const QModelIndex& index)
    : QKeySequenceEdit(parent)
{
    auto all = getActions().all();
    if (index.row() >= 0 && index.row() < all.size())
        action_ = all[index.row()];

    if (action_) {
        const int shortcutIndex = index.column() - 2;
        const QStringList shortcuts = action_->get_shortcuts();
        if (shortcutIndex >= 0 && shortcutIndex < shortcuts.size())
            oldValue_ = shortcuts[shortcutIndex];
        else
            oldValue_.clear();

        setKeySequence(QKeySequence(oldValue_));
    }

    setClearButtonEnabled(true);
    setMaximumSequenceLength(1);
    connect(this, &QKeySequenceEdit::editingFinished,
            this, &KeyboardShortcutsEditor::onEditingFinished);
}

void KeyboardShortcutsEditor::onEditingFinished()
{
    const QString shortcut = keySequence().toString();

    // Workaround for QTBUG-40 double-emit.
    if (shortcut == lastCalledWith_)
        return;

    conflictingRow_ = -1;
    lastCalledWith_ = shortcut;

    if (shortcut.isEmpty())
        return;

    auto all = getActions().all();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i] == action_)
            continue;
        if (all[i]->get_shortcuts().contains(shortcut)) {
            QString txt = all[i]->text;
            txt.remove(QLatin1Char('&'));
            if (txt.endsWith(QLatin1String("...")))
                txt.chop(3);

            const auto reply = QMessageBox::question(
                this,
                tr("Shortcut Conflict"),
                tr("This shortcut is already assigned to \"%1\". "
                   "Do you want to remove it from there?").arg(txt),
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                conflictingRow_ = i;
            } else {
                setKeySequence(QKeySequence(oldValue_));
            }
            return;
        }
    }
}

// ─── KeyboardShortcutsDelegate ────────────────────────────────────────────────

KeyboardShortcutsDelegate::KeyboardShortcutsDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

QWidget* KeyboardShortcutsDelegate::createEditor(QWidget* parent,
                                                  const QStyleOptionViewItem& /*option*/,
                                                  const QModelIndex& index) const
{
    return new KeyboardShortcutsEditor(parent, index);
}

void KeyboardShortcutsDelegate::setModelData(QWidget* editor,
                                              QAbstractItemModel* model,
                                              const QModelIndex& index) const
{
    auto* kbEditor = qobject_cast<KeyboardShortcutsEditor*>(editor);
    if (!kbEditor)
        return;

    auto* proxy = qobject_cast<KeyboardShortcutsProxy*>(model);
    if (!proxy)
        return;

    proxy->setDataEx(index, kbEditor->keySequence(), Qt::EditRole,
                     kbEditor->conflictingRow());
}

// ─── KeyboardShortcutsModel ───────────────────────────────────────────────────

KeyboardShortcutsModel::KeyboardShortcutsModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

int KeyboardShortcutsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(getActions().all().size());
}

int KeyboardShortcutsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return HEADER.size();
}

QVariant KeyboardShortcutsModel::headerData(int section, Qt::Orientation orientation,
                                             int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    if (section < 0 || section >= HEADER.size())
        return {};
    return HEADER[section];
}

QVariant KeyboardShortcutsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    auto all = getActions().all();
    if (index.row() >= all.size())
        return {};

    const Action* action = all[index.row()];
    const int col = index.column();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (col == 0) {
            QString txt = action->text;
            txt.remove(QLatin1Char('&'));
            return txt;
        }
        if (col == 1)
            return action->shortcutsChanged() ? QStringLiteral("✎") : QString{};
        if (col >= 2)
            return QVariant::fromValue(action->getKeySequence(col - 2));
    }

    if (role == Qt::ToolTipRole) {
        if (!action->shortcutsChanged())
            return {};
        if (col == 1)
            return tr("Changed from default");
        if (col >= 2) {
            const QString def = action->getDefaultShortcut(col - 2);
            return tr("Default: %1").arg(def.isEmpty() ? tr("Not set") : def);
        }
    }

    return {};
}

Qt::ItemFlags KeyboardShortcutsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    const Qt::ItemFlags base = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
    if (index.column() <= 1)
        return base;
    return base | Qt::ItemIsEditable;
}

bool KeyboardShortcutsModel::setData(const QModelIndex& index, const QVariant& value,
                                      int role)
{
    const QKeySequence keySeq = value.value<QKeySequence>();
    return setDataEx(index, keySeq, role, -1);
}

bool KeyboardShortcutsModel::setDataEx(const QModelIndex& index,
                                        const QKeySequence& keySeq,
                                        int role, int removeFromOtherRow)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    if (index.column() < 2)
        return false;

    auto all = getActions().all();
    if (index.row() >= all.size())
        return false;

    Action* action = all[index.row()];
    QStringList shortcuts = action->get_shortcuts();

    // Extend to cover the target column.
    while (shortcuts.size() < index.column() - 1)
        shortcuts.append(QString{});

    const int shortcutIndex = index.column() - 2;
    if (shortcutIndex >= shortcuts.size())
        shortcuts.append(keySeq.toString());
    else
        shortcuts[shortcutIndex] = keySeq.toString();

    // Remove empty entries.
    shortcuts.removeAll(QString{});

    // Deduplicate while preserving order.
    QStringList unique;
    QSet<QString> seen;
    // TODO:
    // for (const auto& s : shortcuts) {
    //     if (seen.insert(s).second)
    //         unique.append(s);
    // }

    action->setShortcuts(unique);

    // Emit dataChanged for all shortcut columns.
    emit dataChanged(this->index(index.row(), 1),
                     this->index(index.row(), columnCount() - 1));

    // Remove the shortcut from the conflicting action if requested.
    if (removeFromOtherRow >= 0 && removeFromOtherRow < all.size()) {
        const QString shortcutStr = keySeq.toString();
        Action* other = all[removeFromOtherRow];
        QStringList otherShortcuts = other->get_shortcuts();
        otherShortcuts.removeAll(shortcutStr);
        other->setShortcuts(otherShortcuts);

        emit dataChanged(this->index(removeFromOtherRow, 1),
                         this->index(removeFromOtherRow, columnCount() - 1));
    }

    return true;
}

// ─── KeyboardShortcutsProxy ───────────────────────────────────────────────────

KeyboardShortcutsProxy::KeyboardShortcutsProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new KeyboardShortcutsModel(this));
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(0);
}

bool KeyboardShortcutsProxy::setDataEx(const QModelIndex& proxyIndex,
                                        const QKeySequence& keySeq,
                                        int role, int removeFromOtherRow)
{
    auto* src = qobject_cast<KeyboardShortcutsModel*>(sourceModel());
    if (!src)
        return false;

    // Map the proxy index to source index.
    const QModelIndex srcIndex = mapToSource(proxyIndex);

    // Map the removeFromOtherRow from proxy row to source row if needed.
    int srcRemoveRow = -1;
    if (removeFromOtherRow >= 0) {
        // removeFromOtherRow is a source row index (from getActions().all()).
        srcRemoveRow = removeFromOtherRow;
    }

    return src->setDataEx(srcIndex, keySeq, role, srcRemoveRow);
}

// ─── KeyboardShortcutsView ────────────────────────────────────────────────────

KeyboardShortcutsView::KeyboardShortcutsView(QWidget* parent)
    : QTableView(parent)
{
    setMinimumSize(400, 200);
    setItemDelegate(new KeyboardShortcutsDelegate(this));
    setShowGrid(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    auto* proxy = new KeyboardShortcutsProxy(this);
    setModel(proxy);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(&SettingsEvents::instance(), &SettingsEvents::restoreKeyboardDefaults,
            this, &KeyboardShortcutsView::onRestoreDefaults);
}

void KeyboardShortcutsView::onRestoreDefaults()
{
    viewport()->update();
}

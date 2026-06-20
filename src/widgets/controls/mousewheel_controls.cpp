#include "mousewheel_controls.h"

#include "core/controls.h"
#include "core/settings.h"

#include <QHeaderView>
#include <QVariant>

// ─── MouseWheelModifiersEditor ────────────────────────────────────────────────

MouseWheelModifiersEditor::MouseWheelModifiersEditor(QWidget* parent,
                                                     const QModelIndex& index)
    : MouseControlsEditorBase(parent)
{
    const auto& list = KeyboardSettings::mousewheelActions();
    if (index.row() >= 0 && index.row() < list.size())
        currentAction_ = &list[index.row()];

    if (currentAction_) {
        oldModifiers_ = currentAction_->getModifiers();
        setWindowTitle(tr("MouseWheel Controls for: %1").arg(currentAction_->text()));
    }

    initModifiersInput();
    initButtonRow();

    if (currentAction_)
        setModifiers(oldModifiers_);

    show();
}

int MouseWheelModifiersEditor::findConflictingRow() const
{
    if (!currentAction_)
        return -1;

    MouseWheelConfig temp(QString{}, QString{}, QString{}, getModifiers(), false);
    const auto& list = KeyboardSettings::mousewheelActions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i] == *currentAction_)
            continue;
        if (list[i].conflictsWith(temp))
            return i;
    }
    return -1;
}

QString MouseWheelModifiersEditor::conflictingActionText(int row) const
{
    const auto& list = KeyboardSettings::mousewheelActions();
    if (row >= 0 && row < list.size())
        return list[row].text();
    return {};
}

void MouseWheelModifiersEditor::resetInputs()
{
    setModifiers(oldModifiers_);
}

// ─── MouseWheelDelegate ───────────────────────────────────────────────────────

MouseWheelDelegate::MouseWheelDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

QWidget* MouseWheelDelegate::createEditor(QWidget* parent,
                                           const QStyleOptionViewItem& /*option*/,
                                           const QModelIndex& index) const
{
    QWidget* wrapper = new QWidget(parent);

    MouseWheelModifiersEditor* editor = new MouseWheelModifiersEditor(wrapper, index);

    auto* proxy = qobject_cast<MouseWheelProxy*>(
        const_cast<QAbstractItemModel*>(index.model()));

    QObject::connect(editor, &MouseWheelModifiersEditor::saved,
                     [editor, proxy, index]() {
                         if (!proxy)
                             return;
                         const QVariant data =
                             QVariant::fromValue(editor->getModifiers());
                         proxy->setDataEx(index, data, Qt::EditRole,
                                          editor->conflictingRow());
                     });

    wrapper->setProperty("editor", QVariant::fromValue(static_cast<QObject*>(editor)));
    return wrapper;
}

void MouseWheelDelegate::setModelData(QWidget* /*editor*/,
                                       QAbstractItemModel* /*model*/,
                                       const QModelIndex& /*index*/) const
{
    // The saved signal already called setDataEx on the proxy.
}

// ─── MouseWheelModel ──────────────────────────────────────────────────────────

MouseWheelModel::MouseWheelModel(QObject* parent)
    : MouseControlsModelBase(
          {COL_ACTION, COL_CHANGED, COL_MODIFIERS, COL_INVERTED},
          parent)
{}

int MouseWheelModel::actionCount() const
{
    return static_cast<int>(KeyboardSettings::mousewheelActions().size());
}

QString MouseWheelModel::actionText(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].text();
}

bool MouseWheelModel::actionControlsChanged(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].controlsChanged();
}

QStringList MouseWheelModel::actionModifiers(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].getModifiers();
}

bool MouseWheelModel::actionInverted(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].getInverted();
}

bool MouseWheelModel::actionConfigured(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].isConfigured();
}

bool MouseWheelModel::actionInvertible(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].isInvertible();
}

void MouseWheelModel::setDataOnAction(int row, const QVariant& value)
{
    KeyboardSettings::mousewheelActions()[row].setModifiers(value.toStringList());
}

void MouseWheelModel::setActionInverted(int row, bool inverted)
{
    KeyboardSettings::mousewheelActions()[row].setInverted(inverted);
}

void MouseWheelModel::removeActionControls(int row)
{
    KeyboardSettings::mousewheelActions()[row].removeControls();
}

QStringList MouseWheelModel::actionDefaultModifiers(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].defaultModifiers();
}

bool MouseWheelModel::actionDefaultInverted(int row) const
{
    return KeyboardSettings::mousewheelActions()[row].defaultInverted();
}

// ─── MouseWheelProxy ──────────────────────────────────────────────────────────

MouseWheelProxy::MouseWheelProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new MouseWheelModel(this));
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(0);
}

bool MouseWheelProxy::setDataEx(const QModelIndex& proxyIndex, const QVariant& value,
                                 int role, int removeFromOtherRow)
{
    auto* src = qobject_cast<MouseWheelModel*>(sourceModel());
    if (!src)
        return false;

    const QModelIndex srcIndex = mapToSource(proxyIndex);
    return src->setDataEx(srcIndex, value, role, removeFromOtherRow);
}

// ─── MouseWheelView ───────────────────────────────────────────────────────────

MouseWheelView::MouseWheelView(QWidget* parent)
    : QTableView(parent)
{
    setMinimumSize(400, 200);
    setItemDelegate(new MouseWheelDelegate(this));
    setShowGrid(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    auto* proxy = new MouseWheelProxy(this);
    setModel(proxy);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(&SettingsEvents::instance(), &SettingsEvents::restoreKeyboardDefaults,
            this, &MouseWheelView::onRestoreDefaults);
}

void MouseWheelView::onRestoreDefaults()
{
    viewport()->update();
}

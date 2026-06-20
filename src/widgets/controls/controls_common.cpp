#include "controls_common.h"

#include "core/controls.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

// ─── MouseControlsEditorBase ──────────────────────────────────────────────────

MouseControlsEditorBase::MouseControlsEditorBase(QWidget* parent)
    : QDialog(parent)
{
    setAutoFillBackground(true);
    mainLayout_ = new QVBoxLayout(this);
    setLayout(mainLayout_);
    setModal(true);
}

void MouseControlsEditorBase::initModifiersInput()
{
    QGroupBox* box = new QGroupBox(tr("Modifiers"), this);
    QVBoxLayout* layout = new QVBoxLayout(box);

    for (const auto& [name, flag] : MouseConfigBase::modifierMap()) {
        QCheckBox* cb = new QCheckBox(name, box);
        checkboxes_[name] = cb;
        layout->addWidget(cb);
        connect(cb, &QCheckBox::stateChanged, this, [this, name](int value) {
            onModifiersChanged(name, value);
        });
    }

    mainLayout_->addWidget(box);
}

void MouseControlsEditorBase::initButtonRow()
{
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MouseControlsEditorBase::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout_->addWidget(buttons);
}

void MouseControlsEditorBase::setModifiersNoModifier()
{
    // Called inside ignoreOnChanged_ = true block; block signals to avoid recursion.
    for (auto it = checkboxes_.begin(); it != checkboxes_.end(); ++it) {
        QCheckBox* cb = it.value();
        const QSignalBlocker blocker(cb);
        cb->setChecked(it.key() == QLatin1String("No Modifier"));
    }
}

void MouseControlsEditorBase::onModifiersChanged(const QString& modifier, int value)
{
    if (ignoreOnChanged_)
        return;

    const bool checked = (value == Qt::Checked);
    ignoreOnChanged_ = true;

    if (checked && modifier == QLatin1String("No Modifier")) {
        setModifiersNoModifier();
    }

    if (checked && modifier != QLatin1String("No Modifier")) {
        if (checkboxes_.contains(QLatin1String("No Modifier")))
            checkboxes_[QLatin1String("No Modifier")]->setChecked(false);
    }

    if (!checked && getModifiers(/*cleaned=*/false).isEmpty()) {
        setModifiersNoModifier();
    }

    ignoreOnChanged_ = false;
}

QStringList MouseControlsEditorBase::getModifiers(bool cleaned) const
{
    QStringList result;
    for (const auto& [name, flag] : MouseConfigBase::modifierMap()) {
        if (checkboxes_.contains(name) && checkboxes_[name]->isChecked())
            result.append(name);
    }
    if (cleaned && result.contains(QLatin1String("No Modifier")))
        return {QStringLiteral("No Modifier")};
    return result;
}

void MouseControlsEditorBase::setModifiers(const QStringList& modifiers)
{
    for (auto it = checkboxes_.begin(); it != checkboxes_.end(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setChecked(modifiers.contains(it.key()));
    }
}

void MouseControlsEditorBase::onSave()
{
    conflictingRow_ = findConflictingRow();

    if (conflictingRow_ >= 0) {
        const QString conflictText = conflictingActionText(conflictingRow_);
        const auto reply = QMessageBox::question(
            this,
            tr("Controls Conflict"),
            tr("Do you want to remove the conflicting controls from \"%1\"?")
                .arg(conflictText),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            accept();
            emit saved();
        } else {
            conflictingRow_ = -1;
            resetInputs();
        }
    } else {
        accept();
        emit saved();
    }
}

// ─── MouseControlsModelBase ───────────────────────────────────────────────────

MouseControlsModelBase::MouseControlsModelBase(QList<int> columns, QObject* parent)
    : QAbstractTableModel(parent)
    , columns_(std::move(columns))
{}

int MouseControlsModelBase::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return actionCount();
}

int MouseControlsModelBase::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return columns_.size();
}

QVariant MouseControlsModelBase::headerData(int section, Qt::Orientation orientation,
                                             int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    if (section < 0 || section >= columns_.size())
        return {};

    static const QMap<int, QString> headers = {
        {COL_ACTION,    QStringLiteral("Action")},
        {COL_CHANGED,   QStringLiteral("✎")},
        {COL_BUTTON,    QStringLiteral("Button")},
        {COL_MODIFIERS, QStringLiteral("Modifiers")},
        {COL_INVERTED,  QStringLiteral("Inverted")},
    };
    return headers.value(columns_[section]);
}

Qt::ItemFlags MouseControlsModelBase::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    const int col = columns_.value(index.column(), -1);
    const Qt::ItemFlags base = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;

    if (col == COL_ACTION || col == COL_CHANGED)
        return base;

    if (col == COL_BUTTON || col == COL_MODIFIERS)
        return base | Qt::ItemIsEditable;

    if (col == COL_INVERTED) {
        const int row = index.row();
        if (actionInvertible(row) && actionConfigured(row))
            return base | Qt::ItemIsEditable | Qt::ItemIsUserCheckable;
        return base;
    }

    return base;
}

QVariant MouseControlsModelBase::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    const int row = index.row();
    const int col = columns_.value(index.column(), -1);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
        case COL_ACTION:
            return actionText(row);
        case COL_CHANGED:
            return actionControlsChanged(row) ? QStringLiteral("✎") : QString{};
        case COL_BUTTON:
            return actionButton(row);
        case COL_MODIFIERS:
            return actionModifiers(row).join(QStringLiteral(" + "));
        case COL_INVERTED:
            if (actionConfigured(row) && actionInvertible(row))
                return actionInverted(row) ? tr("Yes") : tr("No");
            return {};
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        if (!actionControlsChanged(row))
            return {};
        switch (col) {
        case COL_CHANGED:
            return tr("Changed from default");
        case COL_BUTTON:
            return tr("Default: %1").arg(actionDefaultButton(row));
        case COL_MODIFIERS:
            return tr("Default: %1").arg(actionDefaultModifiers(row).join(QStringLiteral(" + ")));
        case COL_INVERTED:
            if (actionInvertible(row))
                return tr("Default: %1").arg(actionDefaultInverted(row) ? tr("Yes") : tr("No"));
            return {};
        default:
            return {};
        }
    }

    if (role == Qt::CheckStateRole) {
        if (col == COL_INVERTED && actionConfigured(row) && actionInvertible(row))
            return actionInverted(row) ? Qt::Checked : Qt::Unchecked;
    }

    return {};
}

bool MouseControlsModelBase::setData(const QModelIndex& index, const QVariant& value,
                                      int role)
{
    return setDataEx(index, value, role, -1);
}

bool MouseControlsModelBase::setDataEx(const QModelIndex& index, const QVariant& value,
                                        int role, int removeFromOtherRow)
{
    if (!index.isValid())
        return false;

    const int row = index.row();
    const int col = columns_.value(index.column(), -1);

    if (col == COL_INVERTED) {
        if (role != Qt::CheckStateRole && role != Qt::EditRole)
            return false;
        const bool inverted = (value.toInt() == Qt::Checked);
        setActionInverted(row, inverted);
    } else {
        if (role != Qt::EditRole)
            return false;
        setDataOnAction(row, value);

        if (removeFromOtherRow >= 0) {
            removeActionControls(removeFromOtherRow);
            const QModelIndex topLeft = this->index(removeFromOtherRow, 0);
            const QModelIndex bottomRight = this->index(removeFromOtherRow, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight);
        }
    }

    const QModelIndex topLeft = this->index(row, 0);
    const QModelIndex bottomRight = this->index(row, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight);
    return true;
}

#include "mouse_controls.h"

#include "core/controls.h"
#include "core/settings.h"

#include <QHeaderView>
#include <QLabel>
#include <QVariantMap>
#include <QVBoxLayout>

// ─── MouseControlsEditor ─────────────────────────────────────────────────────

MouseControlsEditor::MouseControlsEditor(QWidget* parent, const QModelIndex& index)
    : MouseControlsEditorBase(parent)
{
    const auto& list = KeyboardSettings::mouseActions();
    if (index.row() >= 0 && index.row() < list.size())
        currentAction_ = &list[index.row()];

    if (currentAction_) {
        oldButton_    = currentAction_->getButton();
        oldModifiers_ = currentAction_->getModifiers();
        setWindowTitle(tr("Mouse Controls for: %1").arg(currentAction_->text()));
    }

    // Button selector.
    mainLayout_->addWidget(new QLabel(tr("Mouse Button:"), this));
    buttonInput_ = new QComboBox(this);
    for (const auto& [name, flag] : MouseConfigBase::buttonMap())
        buttonInput_->addItem(name);
    mainLayout_->addWidget(buttonInput_);

    // Set initial button.
    if (currentAction_)
        setButton(oldButton_);

    initModifiersInput();
    initButtonRow();

    // Set initial modifiers.
    if (currentAction_)
        setModifiers(oldModifiers_);

    connect(buttonInput_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MouseControlsEditor::onButtonChanged);

    // Initialize modifier enabled state.
    onButtonChanged();

    show();
}

QString MouseControlsEditor::getButton() const
{
    const auto& bmap = MouseConfigBase::buttonMap();
    const int idx = buttonInput_->currentIndex();
    if (idx >= 0 && idx < bmap.size())
        return bmap[idx].first;
    return {};
}

QStringList MouseControlsEditor::getModifiers(bool cleaned) const
{
    if (cleaned && getButton() == QLatin1String("Not Configured"))
        return {};
    return MouseControlsEditorBase::getModifiers(cleaned);
}

void MouseControlsEditor::onButtonChanged()
{
    ignoreOnChanged_ = true;
    if (getButton() == QLatin1String("Not Configured")) {
        // Uncheck all modifiers.
        for (auto it = checkboxes_.begin(); it != checkboxes_.end(); ++it)
            it.value()->setChecked(false);
        setModifiersEnabled(false);
    } else {
        if (MouseControlsEditorBase::getModifiers(/*cleaned=*/false).isEmpty())
            setModifiersNoModifier();
        setModifiersEnabled(true);
    }
    ignoreOnChanged_ = false;
}

void MouseControlsEditor::setModifiersEnabled(bool enabled)
{
    for (auto it = checkboxes_.begin(); it != checkboxes_.end(); ++it)
        it.value()->setEnabled(enabled);
}

void MouseControlsEditor::setButton(const QString& value)
{
    const auto& bmap = MouseConfigBase::buttonMap();
    for (int i = 0; i < bmap.size(); ++i) {
        if (bmap[i].first == value) {
            buttonInput_->setCurrentIndex(i);
            return;
        }
    }
}

int MouseControlsEditor::findConflictingRow() const
{
    if (!currentAction_)
        return -1;

    MouseConfig temp(QString{}, QString{}, QString{},
                     getButton(), getModifiers(), false);
    const auto& list = KeyboardSettings::mouseActions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i] == *currentAction_)
            continue;
        if (list[i].conflictsWith(temp))
            return i;
    }
    return -1;
}

QString MouseControlsEditor::conflictingActionText(int row) const
{
    const auto& list = KeyboardSettings::mouseActions();
    if (row >= 0 && row < list.size())
        return list[row].text();
    return {};
}

void MouseControlsEditor::resetInputs()
{
    setButton(oldButton_);
    setModifiers(oldModifiers_);
}

// ─── MouseDelegate ────────────────────────────────────────────────────────────

MouseDelegate::MouseDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

QWidget* MouseDelegate::createEditor(QWidget* parent,
                                      const QStyleOptionViewItem& /*option*/,
                                      const QModelIndex& index) const
{
    QWidget* wrapper = new QWidget(parent);

    MouseControlsEditor* editor = new MouseControlsEditor(wrapper, index);

    auto* proxy = qobject_cast<MouseProxy*>(
        const_cast<QAbstractItemModel*>(index.model()));

    QObject::connect(editor, &MouseControlsEditor::saved,
                     [editor, proxy, index]() {
                         if (!proxy)
                             return;
                         QVariantMap data;
                         data[QStringLiteral("button")] = editor->getButton();
                         data[QStringLiteral("modifiers")] =
                             QVariant::fromValue(editor->getModifiers());
                         proxy->setDataEx(index, data, Qt::EditRole,
                                          editor->conflictingRow());
                     });

    // Store the editor pointer on the wrapper so setModelData can retrieve it.
    wrapper->setProperty("editor", QVariant::fromValue(static_cast<QObject*>(editor)));
    return wrapper;
}

void MouseDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                  const QModelIndex& index) const
{
    QObject* editorObj = editor->property("editor").value<QObject*>();
    auto* mouseEditor = qobject_cast<MouseControlsEditor*>(editorObj);
    if (!mouseEditor)
        return;
    // Only commit if the editor was accepted (saved signal triggered the proxy call).
    // The saved signal already called setDataEx; nothing more to do here.
    Q_UNUSED(model)
    Q_UNUSED(index)
}

// ─── MouseModel ───────────────────────────────────────────────────────────────

MouseModel::MouseModel(QObject* parent)
    : MouseControlsModelBase(
          {COL_ACTION, COL_CHANGED, COL_BUTTON, COL_MODIFIERS, COL_INVERTED},
          parent)
{}

int MouseModel::actionCount() const
{
    return static_cast<int>(KeyboardSettings::mouseActions().size());
}

QString MouseModel::actionText(int row) const
{
    return KeyboardSettings::mouseActions()[row].text();
}

bool MouseModel::actionControlsChanged(int row) const
{
    return KeyboardSettings::mouseActions()[row].controlsChanged();
}

QString MouseModel::actionButton(int row) const
{
    return KeyboardSettings::mouseActions()[row].getButton();
}

QStringList MouseModel::actionModifiers(int row) const
{
    return KeyboardSettings::mouseActions()[row].getModifiers();
}

bool MouseModel::actionInverted(int row) const
{
    return KeyboardSettings::mouseActions()[row].getInverted();
}

bool MouseModel::actionConfigured(int row) const
{
    return KeyboardSettings::mouseActions()[row].isConfigured();
}

bool MouseModel::actionInvertible(int row) const
{
    return KeyboardSettings::mouseActions()[row].isInvertible();
}

void MouseModel::setDataOnAction(int row, const QVariant& value)
{
    const QVariantMap map = value.toMap();
    const auto& list = KeyboardSettings::mouseActions();
    list[row].setButton(map[QStringLiteral("button")].toString());
    list[row].setModifiers(map[QStringLiteral("modifiers")].toStringList());
}

void MouseModel::setActionInverted(int row, bool inverted)
{
    KeyboardSettings::mouseActions()[row].setInverted(inverted);
}

void MouseModel::removeActionControls(int row)
{
    KeyboardSettings::mouseActions()[row].removeControls();
}

QString MouseModel::actionDefaultButton(int row) const
{
    return KeyboardSettings::mouseActions()[row].defaultButton();
}

QStringList MouseModel::actionDefaultModifiers(int row) const
{
    return KeyboardSettings::mouseActions()[row].defaultModifiers();
}

bool MouseModel::actionDefaultInverted(int row) const
{
    return KeyboardSettings::mouseActions()[row].defaultInverted();
}

// ─── MouseProxy ───────────────────────────────────────────────────────────────

MouseProxy::MouseProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new MouseModel(this));
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(0);
}

bool MouseProxy::setDataEx(const QModelIndex& proxyIndex, const QVariant& value,
                            int role, int removeFromOtherRow)
{
    auto* src = qobject_cast<MouseModel*>(sourceModel());
    if (!src)
        return false;

    const QModelIndex srcIndex = mapToSource(proxyIndex);
    return src->setDataEx(srcIndex, value, role, removeFromOtherRow);
}

// ─── MouseView ────────────────────────────────────────────────────────────────

MouseView::MouseView(QWidget* parent)
    : QTableView(parent)
{
    setMinimumSize(400, 200);
    setItemDelegate(new MouseDelegate(this));
    setShowGrid(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    auto* proxy = new MouseProxy(this);
    setModel(proxy);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(&SettingsEvents::instance(), &SettingsEvents::restoreKeyboardDefaults,
            this, &MouseView::onRestoreDefaults);
}

void MouseView::onRestoreDefaults()
{
    viewport()->update();
}

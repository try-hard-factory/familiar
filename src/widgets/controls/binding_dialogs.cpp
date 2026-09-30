#include "binding_dialogs.h"

#include <actions/actions.h>
#include <widgets/dialogs.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

bool same_modifiers(const QStringList& a, const QStringList& b)
{
    return QSet<QString>(a.begin(), a.end())
           == QSet<QString>(b.begin(), b.end());
}

} // namespace

// ─── MouseButtonCaptureField ──────────────────────────────────────────────────

MouseButtonCaptureField::MouseButtonCaptureField(QWidget* parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    update_display();
}

void MouseButtonCaptureField::set_button(const QString& button)
{
    button_ = button;
    update_display();
}

void MouseButtonCaptureField::clear_binding()
{
    button_.clear();
    update_display();
}

void MouseButtonCaptureField::mousePressEvent(QMouseEvent* event)
{
    // Whichever button you click the field with IS the gesture - no
    // separate "arm capture" step, and no modifiers (that's the keyboard
    // field's job). Left-click records Left, middle-click records
    // Middle, etc.
    const auto& bmap = MouseConfigBase::button_map();
    QString name;
    for (const auto& pair : bmap) {
        if (pair.second == event->button() && pair.second != Qt::NoButton) {
            name = pair.first;
            break;
        }
    }

    if (!name.isEmpty()) {
        button_ = name;
        update_display();
    }
    event->accept();
}

void MouseButtonCaptureField::update_display()
{
    if (button_.isEmpty()) {
        setText(QString());
        setPlaceholderText(tr("Click here with a mouse button"));
        return;
    }
    setText(button_ + QStringLiteral(" MB"));
}

// ─── KeySequenceCaptureField ──────────────────────────────────────────────────

KeySequenceCaptureField::KeySequenceCaptureField(QWidget* parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    update_display();
}

void KeySequenceCaptureField::set_sequence(const QString& seq)
{
    sequence_ = seq;
    update_display();
}

void KeySequenceCaptureField::clear_sequence()
{
    sequence_.clear();
    update_display();
}

void KeySequenceCaptureField::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->ignore(); // let the dialog's own Escape-to-close handle it
        return;
    }
    sequence_ = key_event_to_sequence_string(event);
    update_display();
    event->accept();
}

void KeySequenceCaptureField::update_display()
{
    if (sequence_.isEmpty()) {
        setText(QString());
        setPlaceholderText(tr("Click here, then press a key"));
        return;
    }
    setText(sequence_);
}

// ─── BindingEditorDialogBase ──────────────────────────────────────────────────

BindingEditorDialogBase::BindingEditorDialogBase(BindingTarget* target,
                                                 QWidget* parent)
    : QDialog(parent)
    , target_(target)
{
    setAutoFillBackground(true);
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    auto* mouseLabel = new QLabel(tr("Mouse buttons:"), this);
    layout->addWidget(mouseLabel);
    auto* mouseRow = new QHBoxLayout();
    mouseButtonField_ = new MouseButtonCaptureField(this);
    mouseButtonField_->setMinimumWidth(140);
    mouseRow->addWidget(mouseButtonField_);
    auto* mouseClearBtn = new QToolButton(this);
    mouseClearBtn->setText(QStringLiteral("×"));
    connect(mouseClearBtn, &QToolButton::clicked, this, [this] {
        mouseButtonField_->clear_binding();
    });
    mouseRow->addWidget(mouseClearBtn);
    for (const auto& pair : MouseConfigBase::modifier_map()) {
        const QString name = pair.first;
        auto* cb = new QCheckBox(name, this);
        modifierChecks_[name] = cb;
        mouseRow->addWidget(cb);
    }
    layout->addLayout(mouseRow);

    layout->addWidget(new QLabel(tr("Keyboard keys:"), this));
    auto* keyRow = new QHBoxLayout();
    keySequenceField_ = new KeySequenceCaptureField(this);
    keyRow->addWidget(keySequenceField_);
    auto* keyClearBtn = new QToolButton(this);
    keyClearBtn->setText(QStringLiteral("×"));
    connect(keyClearBtn, &QToolButton::clicked, this, [this] {
        keySequenceField_->clear_sequence();
    });
    keyRow->addWidget(keyClearBtn);
    layout->addLayout(keyRow);

    if (target_->is_invertible()) {
        invertCheck_ = new QCheckBox(tr("Invert direction"), this);
        layout->addWidget(invertCheck_);
    }

    // Modifier checkboxes only make sense on their own for
    // MouseWheelControl (no button to capture them together with) -
    // everyone else gets them folded into mouseButtonField_'s capture
    // gesture, or doesn't need them at all.
    for (QCheckBox* cb : std::as_const(modifierChecks_)) {
        cb->setVisible(target_->kind() == BindingTargetKind::MouseWheelControl);
    }

    // Both fields are always editable - Actions can fire from a mouse
    // chord (ActionMouseDispatcher) or a mixed mouse+key chord
    // (HeldButtonsTracker), and Controls can fire from a keyboard key as
    // a single discrete nudge (CanvasView::keyPressEvent). The only
    // genuine constraint left is MouseWheelControl having no button at
    // all - a wheel scroll isn't a click.
    if (target_->kind() == BindingTargetKind::MouseWheelControl) {
        mouseButtonField_->setVisible(false);
        mouseLabel->setVisible(false);
    }
}

void BindingEditorDialogBase::populate_from(const Binding& binding)
{
    mouseButtonField_->set_button(binding.mouseButton);

    for (auto it = modifierChecks_.begin(); it != modifierChecks_.end(); ++it) {
        it.value()->setChecked(binding.mouseModifiers.contains(it.key()));
    }

    // Old-style MouseControl defaults (e.g. Zoom/Pan's hardcoded Binding
    // literals) still carry their modifier in mouseModifiers, not
    // keySequence, since the button-only field can't show it anymore.
    // Surface it in the keyboard field instead so it's visible/editable
    // and survives an Apply without changes - collectBinding() then
    // writes it back as a bare-modifier keySequence, migrating it.
    if (target_->kind() != BindingTargetKind::MouseWheelControl
        && binding.keySequence.isEmpty() && binding.mouseModifiers.size() == 1
        && binding.mouseModifiers.first() != QLatin1String("No Modifier")) {
        keySequenceField_->set_sequence(binding.mouseModifiers.first());
    } else {
        keySequenceField_->set_sequence(binding.keySequence);
    }

    if (invertCheck_) {
        invertCheck_->setChecked(binding.inverted);
    }
}

Binding BindingEditorDialogBase::collect_binding() const
{
    Binding b;

    // Mouse buttons field is button-only (see MouseButtonCaptureField) -
    // a required modifier is either a checkbox (MouseWheelControl, no
    // button to combine it with) or a bare modifier in the keyboard
    // field below (folded in by MouseConfig::matchesEvent).
    if (target_->kind() == BindingTargetKind::MouseWheelControl) {
        for (auto it = modifierChecks_.begin(); it != modifierChecks_.end();
             ++it) {
            if (it.value()->isChecked()) {
                b.mouseModifiers.append(it.key());
            }
        }
        if (b.mouseModifiers.size() > 1
            && b.mouseModifiers.contains(QStringLiteral("No Modifier"))) {
            b.mouseModifiers = {QStringLiteral("No Modifier")};
        }
    } else {
        b.mouseButton = mouseButtonField_->button();
    }

    b.keySequence = keySequenceField_->sequence();

    if (invertCheck_) {
        b.inverted = invertCheck_->isChecked();
    }

    return b;
}

void BindingEditorDialogBase::try_accept()
{
    const Binding candidate = collect_binding();

    // Keyboard part vs. other Actions' shortcuts.
    if (!candidate.keySequence.isEmpty()) {
        if (Action* conflicting
            = get_actions().find_by_shortcut(target_->id(),
                                          candidate.keySequence)) {
            QString txt = conflicting->display_text();
            if (txt.endsWith(QLatin1String("..."))) {
                txt.chop(3);
            }
            const auto reply = show_message_box(
                QMessageBox::Question,
                this,
                tr("Shortcut Conflict"),
                tr("This shortcut is already assigned to \"%1\". "
                   "Do you want to remove it from there?")
                    .arg(txt),
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                return;
            }
            QStringList remaining = conflicting->get_shortcuts();
            remaining.removeAll(candidate.keySequence);
            conflicting->set_shortcuts(remaining);
        }
    }

    // Mouse part vs. other Actions' mouse-chord aliases.
    if (!candidate.mouseButton.isEmpty()) {
        if (Action* conflicting = get_actions().find_by_mouse_binding(target_->id(),
                                                                  candidate)) {
            QString txt = conflicting->display_text();
            if (txt.endsWith(QLatin1String("..."))) {
                txt.chop(3);
            }
            const auto reply
                = show_message_box(QMessageBox::Question,
                                 this,
                                 tr("Shortcut Conflict"),
                                 tr("This is already assigned to \"%1\". "
                                    "Do you want to remove it from there?")
                                     .arg(txt),
                                 QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                return;
            }
            QList<Binding> remaining = conflicting->get_mouse_bindings();
            for (int i = static_cast<int>(remaining.size()) - 1; i >= 0;
                 --i) {
                if (remaining[i].mouseButton == candidate.mouseButton
                    && same_modifiers(remaining[i].mouseModifiers,
                                     candidate.mouseModifiers)) {
                    remaining.removeAt(i);
                }
            }
            conflicting->set_mouse_bindings(remaining);
        }
    }

    // Mouse and/or keyboard part vs. Controls (mouse and wheel groups).
    if (!candidate.mouseButton.isEmpty() || !candidate.keySequence.isEmpty()) {
        KeyboardSettings ks;

        const int mouseRow = ks.find_conflicting_mouse_group(target_->id(),
                                                          candidate);
        if (mouseRow >= 0) {
            const MouseConfig& other
                = KeyboardSettings::mouse_actions()[mouseRow];
            const auto reply = show_message_box(
                QMessageBox::Question,
                this,
                tr("Controls Conflict"),
                tr("Do you want to remove the conflicting controls "
                   "from \"%1\"?")
                    .arg(other.text()),
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                return;
            }
            QList<Binding> theirs = other.get_bindings();
            for (int i = static_cast<int>(theirs.size()) - 1; i >= 0; --i) {
                const bool mouseMatch
                    = !candidate.mouseButton.isEmpty()
                      && theirs[i].mouseButton == candidate.mouseButton
                      && same_modifiers(theirs[i].mouseModifiers,
                                       candidate.mouseModifiers);
                const bool keyMatch = !candidate.keySequence.isEmpty()
                                      && theirs[i].keySequence
                                             == candidate.keySequence;
                if (mouseMatch || keyMatch) {
                    theirs.removeAt(i);
                }
            }
            other.set_bindings(theirs);
        }

        const int wheelRow = ks.find_conflicting_wheel_group(target_->id(),
                                                          candidate);
        if (wheelRow >= 0) {
            const MouseWheelConfig& other
                = KeyboardSettings::mousewheel_actions()[wheelRow];
            const auto reply = show_message_box(
                QMessageBox::Question,
                this,
                tr("Controls Conflict"),
                tr("Do you want to remove the conflicting controls "
                   "from \"%1\"?")
                    .arg(other.text()),
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                return;
            }
            QList<Binding> theirs = other.get_bindings();
            for (int i = static_cast<int>(theirs.size()) - 1; i >= 0; --i) {
                const bool modMatch = !candidate.mouseModifiers.isEmpty()
                                      && same_modifiers(theirs[i].mouseModifiers,
                                                       candidate.mouseModifiers);
                const bool keyMatch = !candidate.keySequence.isEmpty()
                                      && theirs[i].keySequence
                                             == candidate.keySequence;
                if (modMatch || keyMatch) {
                    theirs.removeAt(i);
                }
            }
            other.set_bindings(theirs);
        }
    }

    on_accepted(candidate);
    accept();
}

// ─── AddAliasDialog ───────────────────────────────────────────────────────────

AddAliasDialog::AddAliasDialog(BindingTarget* target, QWidget* parent)
    : BindingEditorDialogBase(target, parent)
{
    setWindowTitle(tr("Add alias for %1").arg(target->text()));
    populate_from(Binding{});

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* applyBtn = buttons->addButton(tr("Apply"),
                                        QDialogButtonBox::AcceptRole);
    connect(applyBtn, &QPushButton::clicked, this, [this] { try_accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout()->addWidget(buttons);
}

void AddAliasDialog::on_accepted(const Binding& candidate)
{
    QList<Binding> all = target_->bindings();
    all.append(candidate);
    target_->set_bindings(all);
}

// ─── RebindDialog ─────────────────────────────────────────────────────────────

RebindDialog::RebindDialog(BindingTarget* target,
                           int bindingIndex,
                           QWidget* parent)
    : BindingEditorDialogBase(target, parent)
    , bindingIndex_(bindingIndex)
{
    setWindowTitle(tr("Rebind %1").arg(target->text()));

    const QList<Binding> current = target->bindings();
    if (bindingIndex_ >= 0 && bindingIndex_ < current.size()) {
        populate_from(current[bindingIndex_]);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);

    const QList<Binding> defaults = target->default_bindings();
    if (bindingIndex_ >= 0 && bindingIndex_ < defaults.size()) {
        auto* defaultBtn = buttons->addButton(tr("Default"),
                                              QDialogButtonBox::ResetRole);
        connect(defaultBtn, &QPushButton::clicked, this, [this, defaults]() {
            populate_from(defaults[bindingIndex_]);
            try_accept();
        });
    }

    auto* applyBtn = buttons->addButton(tr("Apply"),
                                        QDialogButtonBox::AcceptRole);
    connect(applyBtn, &QPushButton::clicked, this, [this] { try_accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout()->addWidget(buttons);
}

void RebindDialog::on_accepted(const Binding& candidate)
{
    QList<Binding> all = target_->bindings();
    if (bindingIndex_ >= 0 && bindingIndex_ < all.size()) {
        all[bindingIndex_] = candidate;
    } else {
        all.append(candidate);
    }
    target_->set_bindings(all);
}

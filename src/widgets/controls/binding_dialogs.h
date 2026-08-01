#pragma once

#include "binding_target.h"

#include <QDialog>
#include <QLineEdit>
#include <QMap>
#include <QString>

class QCheckBox;

// Click-to-capture field for a mouse button, and ONLY a mouse button -
// modifiers are a keyboard concern, captured separately by
// KeySequenceCaptureField (as a bare modifier name like "Alt" when meant
// to be held during the click - see MouseConfig::matchesEvent, which
// folds that in). Whichever button you click the field WITH is the one
// that gets recorded (middle-click the field to bind Middle, right-click
// for Right, etc.) - a single gesture, no separate arm/wait step.
class MouseButtonCaptureField : public QLineEdit
{
    Q_OBJECT

public:
    explicit MouseButtonCaptureField(QWidget* parent = nullptr);

    // Button name from MouseConfigBase::buttonMap() ("Left"/"Middle"/
    // "Right"), or empty if unset.
    QString button() const { return button_; }
    void setButton(const QString& button);
    void clearBinding();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void updateDisplay();

    QString button_;
};

// Click-to-capture field for a keyboard shortcut, including a BARE
// modifier alone (just Ctrl, just Alt, ...) - unlike QKeySequenceEdit,
// which never commits a modifier without a following "real" key. A held-
// modifier-only trigger is a real, useful binding (e.g. "hold Alt to
// pan"), so it's captured here instead. Escape is not captured (falls
// through to the dialog's own close-on-Escape).
class KeySequenceCaptureField : public QLineEdit
{
    Q_OBJECT

public:
    explicit KeySequenceCaptureField(QWidget* parent = nullptr);

    QString sequence() const { return sequence_; }
    void setSequence(const QString& seq);
    void clearSequence();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateDisplay();

    QString sequence_;
};

// Shared UI/logic for "Add alias for X" / "Rebind X": Mouse buttons
// (MouseButtonCaptureField, capturing button+modifiers together - shown
// only for MouseControl targets; MouseWheelControl targets have no button
// at all, so they get the plain modifier checkboxes instead, same
// vocabulary as widgets/controls/controls_common.h's
// MouseControlsEditorBase), Keyboard keys (KeySequenceCaptureField),
// Invert direction (only shown when target->isInvertible()). Both fields
// are always editable for every target kind.
class BindingEditorDialogBase : public QDialog
{
    Q_OBJECT

protected:
    BindingEditorDialogBase(BindingTarget* target, QWidget* parent);

    void populateFrom(const Binding& binding);
    Binding collectBinding() const;

    // Runs conflict detection appropriate to target_->kind() (keyboard
    // shortcuts vs. Action, or button+modifiers vs. other Controls
    // groups), offers to steal the binding from the conflicting owner via
    // showMessageBox, then calls onAccepted() and closes the dialog.
    // Returns without doing anything if the user declines the conflict.
    void tryAccept();
    virtual void onAccepted(const Binding& candidate) = 0;

    BindingTarget* target_;

private:
    MouseButtonCaptureField* mouseButtonField_ = nullptr;
    QMap<QString, QCheckBox*> modifierChecks_; // MouseWheelControl only
    KeySequenceCaptureField* keySequenceField_ = nullptr;
    QCheckBox* invertCheck_ = nullptr;
};

class AddAliasDialog : public BindingEditorDialogBase
{
    Q_OBJECT

public:
    AddAliasDialog(BindingTarget* target, QWidget* parent);

protected:
    void onAccepted(const Binding& candidate) override;
};

class RebindDialog : public BindingEditorDialogBase
{
    Q_OBJECT

public:
    RebindDialog(BindingTarget* target, int bindingIndex, QWidget* parent);

protected:
    void onAccepted(const Binding& candidate) override;

private:
    int bindingIndex_;
};

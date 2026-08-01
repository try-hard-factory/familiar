#pragma once

#include <core/controls.h>

#include <QList>
#include <QString>

struct Action;

enum class BindingTargetKind { Action, MouseControl, MouseWheelControl };

// Adapter so the alias tree/dialogs never need to branch on Action-vs-
// Control directly - wraps either an Action (menu command) or a
// MouseConfigBase (canvas interaction: zoom/pan/...) behind one common
// alias-list interface.
class BindingTarget
{
public:
    virtual ~BindingTarget() = default;

    virtual QString id() const = 0;
    virtual QString text() const = 0;
    virtual BindingTargetKind kind() const = 0;
    virtual bool isInvertible() const { return false; }
    virtual QList<Binding> bindings() const = 0;
    virtual QList<Binding> defaultBindings() const = 0;
    virtual void setBindings(const QList<Binding>& bindings) = 0;

    bool bindingsChanged() const { return bindings() != defaultBindings(); }
};

// Phase 1: keyboard-only (Action doesn't have a mouse-chord binding path
// yet - see memory/familiar_next_steps.md step 6, "Controls" alias UI is
// unified now, mouse-triggered Actions are a later phase).
class ActionBindingTarget : public BindingTarget
{
public:
    explicit ActionBindingTarget(Action* action)
        : action_(action)
    {}

    QString id() const override;
    QString text() const override;
    BindingTargetKind kind() const override
    {
        return BindingTargetKind::Action;
    }
    QList<Binding> bindings() const override;
    QList<Binding> defaultBindings() const override;
    void setBindings(const QList<Binding>& bindings) override;

private:
    Action* action_;
};

class MouseConfigBindingTarget : public BindingTarget
{
public:
    MouseConfigBindingTarget(const MouseConfigBase* config,
                             BindingTargetKind kind)
        : config_(config)
        , kind_(kind)
    {}

    QString id() const override { return config_->id(); }
    QString text() const override { return config_->text(); }
    BindingTargetKind kind() const override { return kind_; }
    bool isInvertible() const override { return config_->isInvertible(); }
    QList<Binding> bindings() const override { return config_->getBindings(); }
    QList<Binding> defaultBindings() const override
    {
        return config_->defaultBindings();
    }
    void setBindings(const QList<Binding>& bindings) override
    {
        config_->setBindings(bindings);
    }

private:
    const MouseConfigBase* config_;
    BindingTargetKind kind_;
};

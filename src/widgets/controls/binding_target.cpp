#include "binding_target.h"

#include <actions/actions.h>

QString ActionBindingTarget::id() const
{
    return action_->id;
}

QString ActionBindingTarget::text() const
{
    QString t = action_->displayText();
    if (t.endsWith(QLatin1String("...")))
        t.chop(3);
    return t;
}

QList<Binding> ActionBindingTarget::bindings() const
{
    QList<Binding> out;
    for (const QString& seq : action_->get_shortcuts())
        out.append(Binding{seq});
    out.append(action_->get_mouse_bindings());
    return out;
}

QList<Binding> ActionBindingTarget::defaultBindings() const
{
    // No Action ships with a default mouse/mixed binding today.
    QList<Binding> out;
    for (const QString& seq : action_->shortcuts)
        out.append(Binding{seq});
    return out;
}

void ActionBindingTarget::setBindings(const QList<Binding>& bindings)
{
    QStringList seqs;
    QList<Binding> mouseBindings;
    for (const Binding& b : bindings) {
        if (!b.mouseButton.isEmpty())
            mouseBindings.append(
                b); // mouse-only or mixed (keeps keySequence too)
        else if (!b.keySequence.isEmpty())
            seqs.append(b.keySequence);
    }
    action_->setShortcuts(seqs);
    action_->setMouseBindings(mouseBindings);
}

#include "held_buttons_tracker.h"

#include <QEvent>
#include <QMouseEvent>

HeldButtonsTracker& HeldButtonsTracker::instance()
{
    static HeldButtonsTracker tracker;
    return tracker;
}

bool HeldButtonsTracker::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        held_ = static_cast<QMouseEvent*>(event)->buttons();
        pressTarget_ = qobject_cast<QWidget*>(watched);
        break;
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
        held_ = static_cast<QMouseEvent*>(event)->buttons();
        break;
    default:
        break;
    }
    return false; // pure observer, never consumes
}

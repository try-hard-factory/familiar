#include "core/held_buttons_tracker.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QWidget>

// HeldButtonsTracker::eventFilter() is protected (only Qt's own event
// dispatch is meant to call it) - exercised here the same way, through
// QCoreApplication::sendEvent() on a widget it's installed on, instead
// of reaching for a friend declaration just for this test.
//
// Singleton (private constructor, one instance for the whole process,
// see held_buttons_tracker.h) - kept to a single TEST with both press
// and release sequenced inside it, same reasoning as
// tests/core/commandline_args_test.cpp's CommandlineArgsTest.
TEST(HeldButtonsTrackerTest, TracksPressAndReleaseViaEventFilter)
{
    QWidget widget;
    HeldButtonsTracker& tracker = HeldButtonsTracker::instance();
    widget.installEventFilter(&tracker);

    // localPos + globalPos overload - the localPos-only one is deprecated
    // since Qt 6.4.
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(0, 0),
                      QPointF(0, 0),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &press);

    EXPECT_EQ(tracker.current(), Qt::MouseButtons(Qt::LeftButton));
    EXPECT_EQ(tracker.pressTarget(), &widget);

    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(0, 0),
                        QPointF(0, 0),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &release);

    EXPECT_EQ(tracker.current(), Qt::MouseButtons(Qt::NoButton));

    widget.removeEventFilter(&tracker);
}

#pragma once

#include <QColor>
#include <QProgressBar>

class QHideEvent;
class QPaintEvent;
class QShowEvent;
class QTimer;

// Hand-painted progress bar, same reasoning as FlatCheckBox/FlatSpinBox/
// FlatComboBox: QSS on a QProgressBar means styling its ::chunk
// sub-control, and this app has been burned twice by QSS sub-control
// rules silently killing a widget's native rendering instead of just
// recoloring it (see QSS subcontrol notes on FlatCheckBox/FlatSpinBox).
// Painting it outright is both safer and the established convention here.
//
// Deliberately text-free (setTextVisible(false) in the ctor, and
// paintEvent() never draws any): the percentage reads better as its own
// label beside the bar than as text fighting the chunk fill for contrast
// - see ProgressDialog (widgets/dialogs.h), which owns that label.
//
// Handles QProgressBar's own "indeterminate" convention (minimum() ==
// maximum() == 0) with a sliding marquee segment, since callers do end
// up in that state for real: an operation that never emits a known total
// (ThreadedIO::beginProcessing) leaves the range at 0..0.
class FlatProgressBar : public QProgressBar
{
    Q_OBJECT

public:
    FlatProgressBar(const QColor& track,
                    const QColor& accent,
                    QWidget* parent = nullptr);

    bool isIndeterminate() const { return minimum() == 0 && maximum() == 0; }

protected:
    void paintEvent(QPaintEvent* event) override;
    // The marquee timer only needs to tick while this widget is actually
    // on screen - a ProgressDialog is hidden far longer than it's shown
    // (it's kept alive and reused between imports, see that class), so
    // leaving a repaint timer running unconditionally would burn cycles
    // for nothing.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QColor track_;
    QColor accent_;
    QTimer* marqueeTimer_ = nullptr;
    // Left edge of the sliding segment, in pixels, only meaningful while
    // isIndeterminate(). Wraps around in the timer's own handler.
    int marqueeOffset_ = 0;
};

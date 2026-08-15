#pragma once

#include <QColor>
#include <QSpinBox>

class QPaintEvent;
class QEnterEvent;
class QResizeEvent;

// Hand-painted QSpinBox: rounded flat-color background (no native box/
// border), custom-drawn up/down triangle glyphs instead of the OS's own
// spin arrows. The number itself is still the real internal QLineEdit's
// own native rendering (an earlier attempt at drawing it here too, with
// the line edit made fully transparent to hide it, backfired: the text
// CURSOR is painted separately from the palette, not affected by a
// QSS "color:" override, so it stayed visible and doubled up with the
// hand-drawn text on top of it - see git history/flat_spinbox.cpp's own
// comments if resurrecting that approach). Two more targeted fixes
// instead:
//  - resizeEvent() pins the line edit's own height to the full widget
//    height - the style's SC_SpinBoxEditField rect came out too short
//    with setFrame(false), clipping/garbling the digits vertically.
//  - selection-color equals the normal text color (selection-background
//    is transparent) - selected text reads identical to unselected, so
//    QAbstractSpinBox's auto-select-all after every value change stops
//    being visible without touching that behavior or the real cursor.
//
// The up/down click-to-step behavior is still the real
// QAbstractSpinBox's own, completely unmodified - paintEvent() positions
// the arrows via the same style()->subControlRect(SC_SpinBoxUp/Down) the
// base class's own mouse handling already keys off internally, so the
// custom drawing and the real hit-test target can't drift apart.
class FlatSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    FlatSpinBox(const QColor& background,
               const QColor& text,
               const QColor& hoverBackground,
               QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QColor background_;
    QColor text_;
    QColor hoverBackground_;
};

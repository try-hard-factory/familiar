#pragma once

#include <QColor>
#include <QDialog>
#include <QWidget>

class QLabel;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;

// Saturation/value square for a fixed hue - horizontal axis is
// saturation (white -> full hue color), vertical is value (fully lit
// at top -> black at bottom), same layout every SV picker uses (GIMP/
// Photoshop and the like). Drag anywhere (mouse press OR move-while-
// pressed) to pick; setHue()/setSv() reposition it programmatically
// (typing a hex value, clicking a preset swatch) without re-emitting
// svChanged() itself - the caller already knows the color it just set.
class SvPicker : public QWidget
{
    Q_OBJECT

public:
    explicit SvPicker(QWidget* parent = nullptr);

    void setHue(int hue);
    void setSv(qreal s, qreal v);

signals:
    void svChanged(qreal s, qreal v);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void pick_(const QPoint& pos);

    int hue_ = 0;
    qreal s_ = 0.0;
    qreal v_ = 1.0;
};

// Horizontal rainbow strip (hue 0-359), round handle, same drag
// convention as SvPicker.
class HueSlider : public QWidget
{
    Q_OBJECT

public:
    explicit HueSlider(QWidget* parent = nullptr);

    void setHue(int hue);
    int hue() const { return hue_; }

signals:
    void hueChanged(int hue);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void pick_(const QPoint& pos);

    int hue_ = 0;
};

// Horizontal transparent-to-opaque strip over a checkerboard (so a
// transparent result is visibly distinct from "no gradient drawn"),
// tinted with the picker's current RGB - setRgb() updates the tint
// whenever hue/saturation/value change elsewhere, independent of this
// slider's own alpha value.
class AlphaSlider : public QWidget
{
    Q_OBJECT

public:
    explicit AlphaSlider(QWidget* parent = nullptr);

    void setRgb(const QColor& rgb);
    void setAlpha(int alpha);

signals:
    void alphaChanged(int alpha);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void pick_(const QPoint& pos);

    QColor rgb_ = Qt::red;
    int alpha_ = 255;
};

// Fixed row of preset swatches (plus a "none"/fully transparent one
// when withAlpha) - click applies that color outright. The swatch
// matching the current color (if any) gets an accent-colored ring, same
// "selection" visual language as the rest of this app's UI.
class SwatchRow : public QWidget
{
    Q_OBJECT

public:
    SwatchRow(bool withNone, const QColor& accent, QWidget* parent = nullptr);

    void setCurrent(const QColor& color);

signals:
    void swatchPicked(QColor color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QRectF cellRect_(int index) const;
    int swatchAt_(const QPoint& pos) const;

    QList<QColor> colors_; // colors_[0] is the "none" swatch iff withNone_
    bool withNone_;
    QColor accent_;
    QColor current_;
};

class ColorPickerDialog : public QDialog
{
    Q_OBJECT

public:
    ColorPickerDialog(QWidget* parent,
                      const QColor& initial,
                      const QString& title,
                      bool withAlpha = true);

    QColor selectedColor() const { return current_; }

signals:
    void colorChanged(QColor color);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    // `source` is skipped when re-syncing every OTHER control, so the
    // control the user is actively dragging/typing into doesn't fight
    // its own edit mid-keystroke (same reentrancy concern as any
    // multi-widget synced-state UI).
    void setColor_(const QColor& color, QObject* source);

    QColor current_;
    bool withAlpha_;

    SvPicker* svPicker_ = nullptr;
    HueSlider* hueSlider_ = nullptr;
    AlphaSlider* alphaSlider_ = nullptr;
    SwatchRow* swatchRow_ = nullptr;
    QLabel* previewSwatch_ = nullptr;
    QLineEdit* hexEdit_ = nullptr;
    QLineEdit* percentEdit_ = nullptr;
};

// Drop-in replacement for the old per-file pickColor() helpers
// (ui/group_toolbar.cpp, ui/text_edit_toolbar.cpp) - modal, returns the
// picked color, or an invalid QColor if cancelled - same semantics as
// the QColorDialog-based versions it replaces.
QColor showColorPickerDialog(QWidget* parent,
                             const QColor& initial,
                             const QString& title,
                             bool withAlpha = true);

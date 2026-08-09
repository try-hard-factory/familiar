#pragma once

#include <QCheckBox>
#include <QColor>

class QPaintEvent;

// Fully self-painted checkbox - outlined square, accent fill + a drawn
// checkmark when checked, drawn text label. Used by this app's custom-
// chrome dialogs (roadmap step 24, Max: "кастомные виджеты для всего
// подряд ... полностью свой дизайн") wherever a themed checkbox is
// needed - currently SaveAllDialog's per-file save list.
//
// Two things were tried and rejected first (Max, both confirmed broken
// via screenshot): (1) a bare QSS stylesheet on a stock QCheckBox -
// setting ANY stylesheet on a QCheckBox switches Qt off native
// indicator rendering entirely, leaving no indicator box at all without
// an explicit ::indicator rule; (2) an explicit
// ::indicator:checked { image: url(data:...) } rule to supply a drawn
// checkmark - Qt's QSS engine doesn't reliably support data: URIs in
// url(), so nothing rendered. This class instead paintEvent()s the
// whole thing itself (same QPainter-icon approach as every other icon
// in this app, see widgets/dialog_style.cpp's severityIcon()), no QSS/
// QStyle involved at all - QAbstractButton's own default hitButton()
// (whole-rect) still makes clicking anywhere on it toggle normally.
class FlatCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    FlatCheckBox(const QString& text,
                const QColor& textColor,
                const QColor& border,
                const QColor& accent,
                QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor textColor_;
    QColor border_;
    QColor accent_;
};

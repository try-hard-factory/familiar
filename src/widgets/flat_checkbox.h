#pragma once

#include <QCheckBox>
#include <QColor>

class QPaintEvent;
class QEnterEvent;

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
    // Hover has no effect on this hand-painted widget by default (no
    // ::hover pseudo-state to key off, unlike a QSS-styled control) -
    // repaint on enter/leave so paintEvent() can check underMouse()
    // itself and lighten the fill a touch as hover feedback.
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QColor textColor_;
    QColor border_;
    QColor accent_;
};

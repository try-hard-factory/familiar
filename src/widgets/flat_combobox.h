#pragma once

#include <QColor>
#include <QComboBox>

class QPaintEvent;
class QEnterEvent;

// Hand-painted QComboBox: rounded flat-color background, custom-drawn
// dropdown arrow instead of the OS's own - same reasoning/precedent as
// FlatSpinBox (widgets/flat_spinbox.h). The popup itself stays
// QSS-styled (settings_style.cpp's own "QComboBox QAbstractItemView"
// rules) - clicking anywhere on the closed box to open it is native
// QComboBox::mousePressEvent behavior, untouched; only the closed box's
// own chrome needed hand-painting.
class FlatComboBox : public QComboBox
{
    Q_OBJECT
public:
    FlatComboBox(const QColor& background,
                const QColor& text,
                const QColor& hoverBackground,
                QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QColor background_;
    QColor text_;
    QColor hoverBackground_;
};

#pragma once

#include <QCheckBox>
#include <QColor>

class QPaintEvent;

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

#ifndef COLORSWIDGET_H
#define COLORSWIDGET_H

#include <QWidget>

class QVBoxLayout;
class ExtendedSlider;

class ColorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ColorsWidget(QWidget* parent = nullptr);
    ~ColorsWidget();

public slots:
    void updateComponents();

signals:
private:
    QVBoxLayout* layout_ = nullptr;
    ExtendedSlider* opacitySlider_ = nullptr;
};

#endif // COLORSWIDGET_H

#ifndef COLORSWIDGET_H
#define COLORSWIDGET_H

#include <QWidget>

class QVBoxLayout;

class ColorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ColorsWidget(QWidget *parent = nullptr);

public slots:
    void updateComponents();

signals:
private:
    QVBoxLayout* layout_ = nullptr;
};

#endif // COLORSWIDGET_H

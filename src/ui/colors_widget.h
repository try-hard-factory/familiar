#ifndef COLORSWIDGET_H
#define COLORSWIDGET_H

#include <QWidget>
#include <kColorPicker/KColorPicker.h>

class QHBoxLayout;
class QVBoxLayout;
class ExtendedSlider;

using kColorPicker::KColorPicker;

class ColorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ColorsWidget(QWidget* parent = nullptr);
    ~ColorsWidget();

private:
    void resetCurrentPreset();
    void labelsInit();
    void presetsInit();
    void colorInit();
    void sliderInit();
    void saveResetBtnsInit();
    void showPresetSaveWindow();

public slots:
    void updateComponents();

signals:

private:
    QVBoxLayout* layout_ = nullptr;
    ExtendedSlider* opacitySlider_ = nullptr;
    QHBoxLayout* header_layout_ = nullptr;
    QHBoxLayout* body_layout_ = nullptr;
    QHBoxLayout* slider_layout_ = nullptr;
    QHBoxLayout* bottom_layout_ = nullptr;
    KColorPicker* background_cp = nullptr;
    KColorPicker* canvas_cp = nullptr;
    KColorPicker* border_cp = nullptr;
    KColorPicker* text_cp = nullptr;
    KColorPicker* selection_cp = nullptr;
};

#endif // COLORSWIDGET_H

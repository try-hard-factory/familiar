#ifndef COLORSWIDGET_H
#define COLORSWIDGET_H

#include <core/settingshandler.h> // EPresetsColorIdx

#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QToolButton;
class QVBoxLayout;
class ExtendedSlider;

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

    void pickColor_(EPresetsColorIdx idx);
    void refreshSwatch_(EPresetsColorIdx idx);

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
    // One button per EPresets value (Dark/Light/Custom1-4), ids matching
    // that enum - see presetsInit().
    QButtonGroup* presetButtons_ = nullptr;
    // Indexed by EPresetsColorIdx.
    QToolButton* colorSwatches_[EPresetsColorIdx::kAllIdx] = {};
};

#endif // COLORSWIDGET_H

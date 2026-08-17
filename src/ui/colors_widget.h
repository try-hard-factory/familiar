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

    // Opens ColorPickerDialog (widgets/color_picker_dialog.h) for `idx` -
    // replaces the old per-swatch KColorPicker. Live-updates the preset
    // (and this swatch's own icon) while dragging via colorChanged(),
    // reverts on cancel - same live-preview-then-commit-or-revert
    // convention GroupToolbar/TextEditToolbar's own color buttons use
    // (ui/group_toolbar.cpp, ui/text_edit_toolbar.cpp).
    void pickColor_(EPresetsColorIdx idx);
    // Redraws colorSwatches_[idx]'s icon from the current preset - shared
    // by pickColor_() and updateComponents() (preset switched out from
    // under this page, e.g. via the preset buttons or Restore Defaults).
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

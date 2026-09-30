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
    void labels_init();
    void presets_init();
    void color_init();
    void slider_init();
    void save_reset_btns_init();
    void show_preset_save_window();

    void pick_color(EPresetsColorIdx idx);
    void refresh_swatch(EPresetsColorIdx idx);

public slots:
    void update_components();

signals:

private:
    QVBoxLayout* layout_ = nullptr;
    ExtendedSlider* opacitySlider_ = nullptr;
    QHBoxLayout* headerLayout_ = nullptr;
    QHBoxLayout* bodyLayout_ = nullptr;
    QHBoxLayout* sliderLayout_ = nullptr;
    QHBoxLayout* bottomLayout_ = nullptr;
    // One button per EPresets value (Dark/Light/Custom1-4), ids matching
    // that enum - see presetsInit().
    QButtonGroup* presetButtons_ = nullptr;
    // Indexed by EPresetsColorIdx.
    QToolButton* colorSwatches_[EPresetsColorIdx::kAllIdx] = {};
};

#endif // COLORSWIDGET_H

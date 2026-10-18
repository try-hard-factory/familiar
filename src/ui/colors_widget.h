#ifndef COLORSWIDGET_H
#define COLORSWIDGET_H

#include <core/settingshandler.h> // EPresetsColorIdx

#include <QMap>
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
    ~ColorsWidget() override;
    // QObject identity is not copyable - QObject itself declares
    // Q_DISABLE_COPY, so copies were already impossible; this states it
    // on the class that has the destructor, which is what the rule of
    // five (and cppcoreguidelines-special-member-functions) asks for.
    Q_DISABLE_COPY_MOVE(ColorsWidget)

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
    // Keyed by EPresetsColorIdx rather than a C array indexed by it: the
    // array had exactly AllIdx (5) slots while AllIdx itself is a legal
    // enum value, so refresh_swatch(AllIdx) indexed one past the end.
    // value() returns nullptr for an absent key, which is what the
    // existing null check in refresh_swatch() already expects.
    QMap<EPresetsColorIdx, QToolButton*> colorSwatches_;
};

#endif // COLORSWIDGET_H

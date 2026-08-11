#ifndef GROUP_TOOLBAR_H
#define GROUP_TOOLBAR_H

/**
 *  @file   group_toolbar.h
 *  \~russian @brief  Плавающая панель управления группой - по образцу
 *                    gif_playback_toolbar.h.
 *                    Lock + выбор цвета заливки + chevron-попап
 *                    с чекбоксом "Drag and drop items into groups".
 *                    Показывается CanvasView'ом, когда выделен ровно
 *                    один GroupItem.
 */

#include <QWidget>

class GroupItem;
class QToolButton;
class QResizeEvent;

class GroupToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit GroupToolbar(QWidget* parent = nullptr);

    // nullptr detaches (the caller hides the widget).
    void attach(GroupItem* item);
    GroupItem* item() const { return item_; }

    // Re-derive the QSS from the current color preset (called on attach
    // and whenever settings change).
    void restyleFromPreset();

signals:
    // Fired whenever the widget's own size changes - see
    // GifPlaybackToolbar::geometryChanged() (same reasoning: CanvasView
    // needs to reposition when internal content resizes this widget
    // without CanvasView itself calling move()).
    void geometryChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    GroupItem* item_ = nullptr;
    QToolButton* lockBtn_ = nullptr;
    QToolButton* fillColorBtn_ = nullptr;
    QToolButton* chevronBtn_ = nullptr;
    QColor iconGlyphColor_;

    // Reused across clicks (see GifPlaybackToolbar::speedPopup_) rather
    // than recreated every time, so repeated clicks don't stack popups.
    QWidget* settingsPopup_ = nullptr;
    void showSettingsPopup_();
    void updateFillColorIcon_();

private slots:
    void onLockToggled_(bool checked);
};

#endif // GROUP_TOOLBAR_H

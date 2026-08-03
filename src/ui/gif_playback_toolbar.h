#ifndef GIF_PLAYBACK_TOOLBAR_H
#define GIF_PLAYBACK_TOOLBAR_H

/**
 *  @file   gif_playback_toolbar.h
 *  \~russian @brief  Плавающая панель управления анимацией GIF
 *                    (PureRef-стиль): пред./след. кадр, play/pause,
 *                    скорость воспроизведения, плёнка всех кадров.
 *                    Показывается CanvasView'ом, когда выделен ровно
 *                    один GifItem.
 */

#include <QWidget>

class GifItem;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;
class QResizeEvent;

class GifPlaybackToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit GifPlaybackToolbar(QWidget* parent = nullptr);

    // nullptr detaches (the caller hides the widget).
    void attach(GifItem* item);
    GifItem* item() const { return item_; }

    // Re-derive the QSS from the current color preset (called on attach
    // and whenever settings change).
    void restyleFromPreset();

    // Positions the control row at `desiredCenterX` (toolbar-local
    // x-coordinate), clamped so it never leaves the toolbar's own bounds.
    // The owner (CanvasView) calls this with the item's center mapped
    // into toolbar-local coordinates - it can't just center the row
    // within the toolbar's own layout, because once the whole toolbar
    // gets clamped against the edge of the viewport (item near the edge
    // of the canvas, wide filmstrip open), the toolbar's own center no
    // longer lines up with the item's center, and a layout-centered row
    // would drift off toward the middle of the (now off-center) filmstrip
    // instead of staying above the item.
    void positionControlsRow(int desiredCenterX);

signals:
    // Fired whenever the widget's own size changes (e.g. the frames
    // filmstrip toggling on/off) - the toolbar is positioned by the owner
    // (CanvasView) via move(), which doesn't get called again just
    // because the layout grew/shrank the widget, so without this the
    // toolbar stays anchored at its old top-left corner and visibly
    // drifts off its centered/clamped spot.
    void geometryChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void showSpeedPopup_();
    void toggleFilmstrip_();
    void rebuildFilmstrip_();
    void onFrameChanged_(int frame);
    void updatePlayPauseIcon_();
    void updateSpeedLabel_();

    GifItem* item_ = nullptr;
    QToolButton* prevBtn_ = nullptr;
    QToolButton* playPauseBtn_ = nullptr;
    QToolButton* nextBtn_ = nullptr;
    QToolButton* speedBtn_ = nullptr;
    QToolButton* framesBtn_ = nullptr;
    // Second row, hidden until framesBtn_ is toggled on - one clickable
    // thumbnail per frame (GifItem::frame_thumbnails()), current frame
    // highlighted.
    QWidget* filmstrip_ = nullptr;
    QHBoxLayout* filmstripLay_ = nullptr;
    QColor iconGlyphColor_;
    // Needed by positionControlsRow() to move controlsRow_ within it.
    QVBoxLayout* outerLay_ = nullptr;
    QWidget* controlsRow_ = nullptr;
    // Speed popup is reused across clicks (see showSpeedPopup_()) instead
    // of being recreated each time - cleared back to nullptr on close via
    // QObject::destroyed (WA_DeleteOnClose still owns its lifetime).
    QWidget* speedPopup_ = nullptr;
};

#endif // GIF_PLAYBACK_TOOLBAR_H

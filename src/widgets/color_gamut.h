#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPaintEvent>
#include <QSize>
#include <QSlider>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include "moveitem.h"
#include <atomic>
#include <optional>

class CanvasScene;
class GamutWidget;

class GamutPainterThread : public QThread
{
    Q_OBJECT

public:
    static constexpr int kradius = 250;

    // Takes an image rather than the PixmapItem it came from: QPixmap
    // must not be touched off the GUI thread, and the item's gamut cache
    // is unsynchronised. "gamut" carries that cache in when the item
    // already has one, so re-rendering at a new threshold skips the
    // histogram entirely.
    GamutPainterThread(GamutWidget* parent,
                       QImage sourceImage,
                       std::optional<PixmapItem::ColorGamut> gamut);

    // Written from the GUI thread while run() may be reading it.
    void set_threshold(int threshold)
    {
        mThreshold_.store(threshold, std::memory_order_relaxed);
    }
    void run() override;

    // Valid to read once the thread has finished - see
    // GamutWidget::on_worker_finished(). Empty when the histogram was
    // never built, or was abandoned half-way on interruption.
    std::optional<PixmapItem::ColorGamut> gamut() const { return mGamut_; }

signals:
    void image_ready(const QImage& image);

private:
    QImage mSourceImage_;
    std::optional<PixmapItem::ColorGamut> mGamut_;
    // Same default the spin box starts at - see color_gamut.cpp.
    static constexpr int kdefaultThreshold = 20;
    std::atomic<int> mThreshold_ = kdefaultThreshold;
};


class GamutWidget : public QWidget
{
    Q_OBJECT

public:
    GamutWidget(QWidget* parent, PixmapItem* item);
    ~GamutWidget() override;
    // QObject identity is not copyable - QObject itself declares
    // Q_DISABLE_COPY, so copies were already impossible; this states it
    // on the class that has the destructor, which is what the rule of
    // five (and cppcoreguidelines-special-member-functions) asks for.
    Q_DISABLE_COPY_MOVE(GamutWidget)

    QSize minimumSizeHint() const override
    {
        // Square: the wheel is drawn inside a square inscribed in
        // whatever space this gets.
        static constexpr int kminSide = 200;
        return {kminSide, kminSide};
    }
    void update_values();
    int threshold() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void on_image_ready(const QImage& image);
    void on_worker_finished();

    // Closes the slots/signals section above - moc needs it, even
    // though to the compiler it repeats the enclosing access level.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    // The picture is referred to by scene + uid rather than by pointer:
    // it can be deleted while this dialog is still open, and
    // CanvasScene::find_by_uid() then simply answers null.
    CanvasScene* mScene_;
    QUuid mItemUid_;
    GamutPainterThread* mWorker_;
    QImage mImage_;
    // Set when the threshold changed while the worker was mid-render -
    // see update_values().
    bool mRerenderPending_ = false;
};


class GamutDialog : public QDialog
{
    Q_OBJECT

public:
    GamutDialog(QWidget* parent, PixmapItem* item);

    int threshold() const { return mThresholdInput_->value(); }

private slots:
    void on_value_changed(int value);

    // Closes the slots/signals section above - moc needs it, even
    // though to the compiler it repeats the enclosing access level.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    GamutWidget* mGamutWidget_;
    QSlider* mThresholdInput_;
};

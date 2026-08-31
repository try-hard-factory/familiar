#include "flat_progress_bar.h"

#include <QHideEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QTimer>

namespace {

// Track thickness. Slim on purpose - the percentage lives in its own
// label next to the bar (see the header), so this doesn't need to be
// tall enough to hold text.
constexpr int kBarHeight = 8;
// One marquee step, in pixels, per timer tick.
constexpr int kMarqueeStepPx = 3;
constexpr int kMarqueeIntervalMs = 16;

} // namespace

FlatProgressBar::FlatProgressBar(const QColor& track,
                                 const QColor& accent,
                                 QWidget* parent)
    : QProgressBar(parent)
    , track_(track)
    , accent_(accent)
{
    setTextVisible(false);
    setFixedHeight(kBarHeight);
    marqueeTimer_ = new QTimer(this);
    marqueeTimer_->setInterval(kMarqueeIntervalMs);
    connect(marqueeTimer_, &QTimer::timeout, this, [this] {
        // Only actually repaints in the indeterminate state - a
        // determinate bar already repaints on every setValue(), so
        // ticking it here would just be redundant work.
        if (!isIndeterminate()) {
            return;
        }
        marqueeOffset_ += kMarqueeStepPx;
        update();
    });
}

void FlatProgressBar::showEvent(QShowEvent* event)
{
    QProgressBar::showEvent(event);
    marqueeTimer_->start();
}

void FlatProgressBar::hideEvent(QHideEvent* event)
{
    QProgressBar::hideEvent(event);
    marqueeTimer_->stop();
    marqueeOffset_ = 0;
}

void FlatProgressBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF full(0, 0, width(), height());
    const qreal radius = full.height() / 2.0;

    // Fully rounded track, then the filled part clipped to that same
    // shape - so the fill picks up the track's rounded ends instead of
    // needing its own (which would look wrong mid-progress, where the
    // fill's right edge should be square against the remaining track).
    QPainterPath trackPath;
    trackPath.addRoundedRect(full, radius, radius);
    painter.fillPath(trackPath, track_);
    painter.setClipPath(trackPath);

    if (isIndeterminate()) {
        const qreal segment = full.width() / 3.0;
        // The segment travels a full width + its own length, so it
        // slides fully off one end before reappearing at the other
        // rather than popping.
        const qreal span = full.width() + segment;
        const qreal x = std::fmod(qreal(marqueeOffset_), span) - segment;
        painter.fillRect(QRectF(x, 0, segment, full.height()), accent_);
        return;
    }

    const int span = maximum() - minimum();
    if (span <= 0) {
        return;
    }
    const qreal fraction = qBound(0.0, (value() - minimum()) / qreal(span), 1.0);
    painter.fillRect(QRectF(0, 0, full.width() * fraction, full.height()),
                     accent_);
}

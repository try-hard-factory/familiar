#include "color_gamut.h"

#include <QActionEvent>
#include <QBoxLayout>
#include <QBrush>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QImage>
#include <QLabel>
#include <QObject>
#include <QPainter>
#include <QPoint>
#include <QRectF>
#include <QSlider>
#include <QThread>
#include <QWidget>
#include <Qt>

#include "canvasscene.h"
#include "moveitem.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

namespace {

// The wheel is drawn at full value - only hue and saturation vary across
// it, so brightness is pinned to the top of the 0-255 range.
constexpr int kfullValue = 255;
// Radius of one plotted sample.
constexpr int ksampleRadius = 3;
// Default "how many pixels of a hue before it counts" threshold, and the
// upper bound of the spin box that edits it.
constexpr int kdefaultThreshold = 20;
constexpr int kmaxThreshold = 500;
// Where the "counting" placeholder text sits while the thread works.
constexpr int kplaceholderX = 10;
constexpr int kplaceholderY = 20;

} // namespace

// GamutPainterThread
// TODOLATER:
GamutPainterThread::GamutPainterThread(
    GamutWidget* parent,
    QImage sourceImage,
    std::optional<PixmapItem::ColorGamut> gamut)
    : QThread(parent)
    , mSourceImage_(std::move(sourceImage))
    , mGamut_(std::move(gamut))
{}

void GamutPainterThread::run()
{
    if (!mGamut_) {
        mGamut_ = PixmapItem::color_gamut_from_image(mSourceImage_, [this] {
            return isInterruptionRequested();
        });
        if (isInterruptionRequested()) {
            // The histogram above bailed out half-built; drop it so the
            // next run starts over instead of caching a partial one.
            mGamut_.reset();
            return;
        }
    }
    const PixmapItem::ColorGamut& gamut = *mGamut_;
    const int threshold = mThreshold_.load(std::memory_order_relaxed);

    QImage image(kradius * 2, kradius * 2, QImage::Format_ARGB32);
    image.fill(QColor(0, 0, 0, 0));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    painter.setBrush(QBrush(Qt::black));
    painter.setPen(Qt::NoPen);

    const QPoint center(kradius, kradius);
    painter.drawEllipse(center, kradius, kradius);

    for (auto it = gamut.constBegin(); it != gamut.constEnd(); ++it) {
        if (it.value() < threshold) {
            continue;
        }
        const int hue = it.key().first;
        const int saturation = it.key().second;
        const double hypotenuse = saturation / 255.0 * kradius;
        const double angle = std::numbers::pi / 180.0 * (-90.0 - hue);
        const int x = int(std::sin(angle) * hypotenuse) + center.x();
        const int y = int(std::cos(angle) * hypotenuse) + center.y();
        QColor color;
        color.setHsv(hue, saturation, kfullValue);
        painter.setBrush(QBrush(color));
        painter.drawEllipse(QPoint(x, y), ksampleRadius, ksampleRadius);
    }

    if (isInterruptionRequested()) {
        return;
    }
    emit image_ready(image);
}


// GamutWidget

GamutWidget::GamutWidget(QWidget* parent, PixmapItem* item)
    : QWidget(parent)
    , mScene_(dynamic_cast<CanvasScene*>(item->scene()))
    , mItemUid_(item->uid())
    // Both arguments touch the item, so they are evaluated here on the
    // GUI thread and the worker only ever sees the results. When the
    // item already has a histogram there is nothing left to walk, so the
    // image conversion - a full-size copy of the pixmap - is skipped.
    , mWorker_(new GamutPainterThread(
          this,
          item->has_color_gamut() ? QImage() : item->gamut_source_image(),
          item->cached_color_gamut()))
{
    connect(mWorker_,
            &GamutPainterThread::image_ready,
            this,
            &GamutWidget::on_image_ready);
    connect(mWorker_,
            &GamutPainterThread::finished,
            this,
            &GamutWidget::on_worker_finished);
    // The worker is deliberately NOT started here. threshold() reads it
    // off the parent GamutDialog, and this constructor runs from that
    // dialog's member-initialiser list - at which point the object's
    // dynamic type is already GamutDialog, so the qobject_cast in
    // threshold() succeeds and dereferences mThresholdInput_, which is
    // declared after mGamutWidget_ and therefore still uninitialised.
    // GamutDialog's constructor calls update_values() once its slider
    // exists, and that both sets the threshold and starts the worker.
}

GamutWidget::~GamutWidget()
{
    // The worker is a child of this widget, so ~QObject would delete it -
    // and QThread's destructor aborts the process when its thread is
    // still running, which is what closing the dialog mid-count did.
    mWorker_->requestInterruption();
    mWorker_->wait();
}

int GamutWidget::threshold() const
{
    auto* dialog = qobject_cast<GamutDialog*>(parentWidget());
    return dialog ? dialog->threshold() : kdefaultThreshold;
}

void GamutWidget::update_values()
{
    mWorker_->set_threshold(threshold());
    if (mWorker_->isRunning()) {
        // run() reads the threshold once, when it starts drawing, so a
        // change arriving now may miss this render entirely. Re-run it
        // as soon as the current pass is done.
        mRerenderPending_ = true;
        return;
    }
    mWorker_->start();
}

void GamutWidget::on_image_ready(const QImage& image)
{
    mImage_ = image;
    update();
}

// Runs on the GUI thread after run() has returned, so reading the
// worker's histogram here races with nothing. Handing it to the item
// means a second GamutDialog on the same picture starts already counted.
void GamutWidget::on_worker_finished()
{
    const std::optional<PixmapItem::ColorGamut> gamut = mWorker_->gamut();
    if (gamut.has_value() && mScene_ != nullptr) {
        auto* item = dynamic_cast<PixmapItem*>(mScene_->find_by_uid(mItemUid_));
        if (item != nullptr && !item->has_color_gamut()) {
            item->cache_color_gamut(*gamut);
        }
    }
    if (mRerenderPending_) {
        mRerenderPending_ = false;
        update_values();
    }
}

void GamutWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);
    if (!mImage_.isNull()) {
        const int size = std::min(width(), height());
        const double x = std::max((width() - size) / 2.0, 0.0);
        const double y = std::max((height() - size) / 2.0, 0.0);
        painter.drawImage(QRectF(x, y, size, size), mImage_);
    } else {
        painter.drawText(kplaceholderX, kplaceholderY, "Counting pixels...");
    }
}


// GamutDialog

GamutDialog::GamutDialog(QWidget* parent, PixmapItem* item)
    : QDialog(parent)
    , mGamutWidget_(new GamutWidget(this, item))
    , mThresholdInput_(new QSlider(this))
{
    // See ChangeOpacityDialog: shown non-modally via show() below and
    // never explicitly deleted by whoever calls "new GamutDialog(...)".
    setAttribute(Qt::WA_DeleteOnClose);
    // See FileActions::openFile(): MainWindow's translucent/frameless
    // stylesheet cascades into this otherwise-unstyled top-level dialog,
    // painting it solid black.
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet("* { background-color: palette(window); color: "
                  "palette(window-text); }");
    setWindowTitle("Color Gamut");

    auto* controlsLayout = new QVBoxLayout();
    controlsLayout->addWidget(new QLabel("Threshold:", this));


    mThresholdInput_->setRange(0, kmaxThreshold);
    mThresholdInput_->setValue(kdefaultThreshold);
    mThresholdInput_->setTracking(false);
    connect(mThresholdInput_,
            &QSlider::valueChanged,
            this,
            &GamutDialog::on_value_changed);
    controlsLayout->addWidget(mThresholdInput_, 0, Qt::AlignHCenter);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    controlsLayout->addWidget(buttons);

    auto* layout = new QHBoxLayout();
    setLayout(layout);


    layout->addWidget(mGamutWidget_, 1);
    layout->addLayout(controlsLayout, 0);

    // Starts the painter thread with the slider's real value - see the
    // comment in GamutWidget's constructor for why it cannot start there.
    mGamutWidget_->update_values();

    show();
}

void GamutDialog::on_value_changed(int /*value*/)
{
    mGamutWidget_->update_values();
}

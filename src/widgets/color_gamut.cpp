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

#include "moveitem.h"
#include <algorithm>
#include <cmath>
#include <numbers>

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
GamutPainterThread::GamutPainterThread(GamutWidget* parent, PixmapItem* item)
    : QThread(parent)
    , mItem_(item)
{}

void GamutPainterThread::run()
{
    const PixmapItem::ColorGamut& gamut = mItem_->color_gamut();

    QImage image(kradius * 2, kradius * 2, QImage::Format_ARGB32);
    image.fill(QColor(0, 0, 0, 0));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    painter.setBrush(QBrush(Qt::black));
    painter.setPen(Qt::NoPen);

    const QPoint center(kradius, kradius);
    painter.drawEllipse(center, kradius, kradius);

    for (auto it = gamut.constBegin(); it != gamut.constEnd(); ++it) {
        if (it.value() < mThreshold_) {
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

    emit image_ready(image);
}


// GamutWidget

GamutWidget::GamutWidget(QWidget* parent, PixmapItem* item)
    : QWidget(parent)
    , mWorker_(new GamutPainterThread(this, item))
{
    connect(mWorker_,
            &GamutPainterThread::image_ready,
            this,
            &GamutWidget::on_image_ready);
    mWorker_->set_threshold(threshold());
    mWorker_->start();
}

int GamutWidget::threshold() const
{
    auto* dialog = qobject_cast<GamutDialog*>(parentWidget());
    return dialog ? dialog->threshold() : kdefaultThreshold;
}

void GamutWidget::update_values()
{
    mWorker_->set_threshold(threshold());
    if (!mWorker_->isRunning()) {
        mWorker_->start();
    }
}

void GamutWidget::on_image_ready(const QImage& image)
{
    mImage_ = image;
    update();
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

    show();
}

void GamutDialog::on_value_changed(int /*value*/)
{
    mGamutWidget_->update_values();
}

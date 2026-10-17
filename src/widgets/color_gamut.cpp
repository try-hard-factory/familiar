#include "color_gamut.h"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QRectF>

#include <algorithm>
#include <cmath>

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
        const double angle = M_PI / 180.0 * (-90.0 - hue);
        const int x = int(std::sin(angle) * hypotenuse) + center.x();
        const int y = int(std::cos(angle) * hypotenuse) + center.y();
        QColor color;
        color.setHsv(hue, saturation, 255);
        painter.setBrush(QBrush(color));
        painter.drawEllipse(QPoint(x, y), 3, 3);
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
    return dialog ? dialog->threshold() : 20;
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
        painter.drawText(10, 20, "Counting pixels...");
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


    mThresholdInput_->setRange(0, 500);
    mThresholdInput_->setValue(20);
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

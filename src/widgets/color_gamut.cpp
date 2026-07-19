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
    , m_item(item)
{}

void GamutPainterThread::run()
{
    const PixmapItem::ColorGamut& gamut = m_item->color_gamut();

    QImage image(RADIUS * 2, RADIUS * 2, QImage::Format_ARGB32);
    image.fill(QColor(0, 0, 0, 0));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    painter.setBrush(QBrush(Qt::black));
    painter.setPen(Qt::NoPen);

    QPoint center(RADIUS, RADIUS);
    painter.drawEllipse(center, RADIUS, RADIUS);

    for (auto it = gamut.constBegin(); it != gamut.constEnd(); ++it) {
        if (it.value() < m_threshold)
            continue;
        int hue = it.key().first;
        int saturation = it.key().second;
        double hypotenuse = saturation / 255.0 * RADIUS;
        double angle = M_PI / 180.0 * (-90.0 - hue);
        int x = int(std::sin(angle) * hypotenuse) + center.x();
        int y = int(std::cos(angle) * hypotenuse) + center.y();
        QColor color;
        color.setHsv(hue, saturation, 255);
        painter.setBrush(QBrush(color));
        painter.drawEllipse(QPoint(x, y), 3, 3);
    }

    emit imageReady(image);
}


// GamutWidget

GamutWidget::GamutWidget(QWidget* parent, PixmapItem* item)
    : QWidget(parent)
    , m_worker(new GamutPainterThread(this, item))
{
    connect(m_worker, &GamutPainterThread::imageReady,
            this, &GamutWidget::onImageReady);
    m_worker->setThreshold(threshold());
    m_worker->start();
}

int GamutWidget::threshold() const
{
    auto* dialog = qobject_cast<GamutDialog*>(parentWidget());
    return dialog ? dialog->threshold() : 20;
}

void GamutWidget::updateValues()
{
    m_worker->setThreshold(threshold());
    if (!m_worker->isRunning())
        m_worker->start();
}

void GamutWidget::onImageReady(const QImage& image)
{
    m_image = image;
    update();
}

void GamutWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);
    if (!m_image.isNull()) {
        int size = std::min(width(), height());
        double x = std::max((width() - size) / 2.0, 0.0);
        double y = std::max((height() - size) / 2.0, 0.0);
        painter.drawImage(QRectF(x, y, size, size), m_image);
    } else {
        painter.drawText(10, 20, "Counting pixels...");
    }
}


// GamutDialog

GamutDialog::GamutDialog(QWidget* parent, PixmapItem* item)
    : QDialog(parent)
{
    setWindowTitle("Color Gamut");

    QVBoxLayout* controlsLayout = new QVBoxLayout();
    controlsLayout->addWidget(new QLabel("Threshold:", this));

    m_thresholdInput = new QSlider(this);
    m_thresholdInput->setRange(0, 500);
    m_thresholdInput->setValue(20);
    m_thresholdInput->setTracking(false);
    connect(m_thresholdInput, &QSlider::valueChanged,
            this, &GamutDialog::onValueChanged);
    controlsLayout->addWidget(m_thresholdInput, 0, Qt::AlignHCenter);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    controlsLayout->addWidget(buttons);

    QHBoxLayout* layout = new QHBoxLayout();
    setLayout(layout);

    m_gamutWidget = new GamutWidget(this, item);
    layout->addWidget(m_gamutWidget, 1);
    layout->addLayout(controlsLayout, 0);

    show();
}

void GamutDialog::onValueChanged(int /*value*/)
{
    m_gamutWidget->updateValues();
}

#include "color_gamut.h"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QRectF>

#include <algorithm>
#include <cmath>

// GamutPainterThread

GamutPainterThread::GamutPainterThread(QObject* parent, const QPixmap& pixmap)
    : QThread(parent)
    , m_pixmap(pixmap)
{}

void GamutPainterThread::computeGamut()
{
    QImage img = m_pixmap.toImage();
    int step = std::max(1, std::max(img.width(), img.height()) / 1000);

    for (int i = 0; i < img.width(); i += step) {
        for (int j = 0; j < img.height(); j += step) {
            QColor rgb = img.pixelColor(i, j);
            int r = rgb.red(), g = rgb.green(), b = rgb.blue();
            int minVal = std::min({r, g, b});
            int maxVal = std::max({r, g, b});
            if (rgb.alpha() > 5 && minVal < 250 && maxVal > 5) {
                int hue = rgb.hsvHue();
                if (hue == -1)
                    continue; // skip achromatic pixels
                m_gamut[hue * 256 + rgb.hsvSaturation()]++;
            }
        }
    }
    m_gamutComputed = true;
}

void GamutPainterThread::run()
{
    if (!m_gamutComputed)
        computeGamut();

    QImage image(RADIUS * 2, RADIUS * 2, QImage::Format_ARGB32);
    image.fill(QColor(0, 0, 0, 0));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    painter.setBrush(QBrush(Qt::black));
    painter.setPen(Qt::NoPen);

    QPoint center(RADIUS, RADIUS);
    painter.drawEllipse(center, RADIUS, RADIUS);

    for (auto it = m_gamut.constBegin(); it != m_gamut.constEnd(); ++it) {
        if (it.value() < m_threshold)
            continue;
        int key = it.key();
        int hue = key / 256;
        int saturation = key % 256;
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

GamutWidget::GamutWidget(QWidget* parent, const QPixmap& pixmap)
    : QWidget(parent)
    , m_worker(new GamutPainterThread(this, pixmap))
{
    connect(m_worker, &GamutPainterThread::imageReady,
            this, &GamutWidget::onImageReady);
    m_worker->start();
}

void GamutWidget::updateValues(int threshold)
{
    m_worker->setThreshold(threshold);
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

GamutDialog::GamutDialog(QWidget* parent, QGraphicsPixmapItem* item)
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

    m_gamutWidget = new GamutWidget(this, item->pixmap());
    layout->addWidget(m_gamutWidget, 1);
    layout->addLayout(controlsLayout, 0);

    show();
}

void GamutDialog::onValueChanged(int value)
{
    m_gamutWidget->updateValues(value);
}

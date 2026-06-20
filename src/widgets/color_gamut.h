#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsPixmapItem>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMap>
#include <QPaintEvent>
#include <QPixmap>
#include <QSize>
#include <QSlider>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

class GamutPainterThread : public QThread
{
    Q_OBJECT

public:
    static constexpr int RADIUS = 250;

    GamutPainterThread(QObject* parent, const QPixmap& pixmap);

    void setThreshold(int threshold) { m_threshold = threshold; }
    void run() override;

signals:
    void imageReady(const QImage& image);

private:
    void computeGamut();

    QPixmap m_pixmap;
    int m_threshold = 20;
    QMap<int, int> m_gamut; // key = hue*256 + saturation
    bool m_gamutComputed = false;
};


class GamutWidget : public QWidget
{
    Q_OBJECT

public:
    GamutWidget(QWidget* parent, const QPixmap& pixmap);

    QSize minimumSizeHint() const override { return QSize(200, 200); }
    void updateValues(int threshold);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onImageReady(const QImage& image);

private:
    GamutPainterThread* m_worker;
    QImage m_image;
};


class GamutDialog : public QDialog
{
    Q_OBJECT

public:
    GamutDialog(QWidget* parent, QGraphicsPixmapItem* item);

private slots:
    void onValueChanged(int value);

private:
    GamutWidget* m_gamutWidget;
    QSlider* m_thresholdInput;
};

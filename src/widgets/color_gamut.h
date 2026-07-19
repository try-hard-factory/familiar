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
#include <QVBoxLayout>
#include <QWidget>

#include "moveitem.h"

class GamutWidget;

class GamutPainterThread : public QThread
{
    Q_OBJECT

public:
    static constexpr int RADIUS = 250;

    GamutPainterThread(GamutWidget* parent, PixmapItem* item);

    void setThreshold(int threshold) { m_threshold = threshold; }
    void run() override;

signals:
    void imageReady(const QImage& image);

private:
    PixmapItem* m_item;
    int m_threshold = 20;
};


class GamutWidget : public QWidget
{
    Q_OBJECT

public:
    GamutWidget(QWidget* parent, PixmapItem* item);

    QSize minimumSizeHint() const override { return QSize(200, 200); }
    void updateValues();
    int threshold() const;

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
    GamutDialog(QWidget* parent, PixmapItem* item);

    int threshold() const { return m_thresholdInput->value(); }

private slots:
    void onValueChanged(int value);

private:
    GamutWidget* m_gamutWidget;
    QSlider* m_thresholdInput;
};

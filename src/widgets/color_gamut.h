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
    static constexpr int kradius = 250;

    GamutPainterThread(GamutWidget* parent, PixmapItem* item);

    void set_threshold(int threshold) { mThreshold_ = threshold; }
    void run() override;

signals:
    void image_ready(const QImage& image);

private:
    PixmapItem* mItem_;
    int mThreshold_ = 20;
};


class GamutWidget : public QWidget
{
    Q_OBJECT

public:
    GamutWidget(QWidget* parent, PixmapItem* item);

    QSize minimumSizeHint() const override { return {200, 200}; }
    void update_values();
    int threshold() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void on_image_ready(const QImage& image);

// Closes the slots/signals section above - moc needs it, even
// though to the compiler it repeats the enclosing access level.
// NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    GamutPainterThread* mWorker_;
    QImage mImage_;
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

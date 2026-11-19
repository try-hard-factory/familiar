#ifndef MODELITEM_H
#define MODELITEM_H

#include "moveitem.h"
#include <memory>
#include <QByteArray>
#include <QTemporaryFile>
#include <QUndoCommand>
#include <QUrl>

// A model is a normal canvas picture while idle. Its pixels are the last
// camera view, so the existing selection, transforms and exporters work.
class ModelItem : public PixmapItem
{
public:
    ModelItem(const QImage& preview,
              QByteArray modelBytes,
              const QString& filename = {});

    std::string get_type() const override { return "model"; }
    IBaseItem* create_copy() override;
    QVariantMap get_extra_save_data() const override;
    void apply_extra_save_data(const QVariantMap& data);

    const QByteArray& model_bytes() const { return modelBytes_; }
    QUrl model_url();
    qreal yaw() const { return yaw_; }
    qreal pitch() const { return pitch_; }
    qreal distance() const { return distance_; }
    void set_view(qreal yaw, qreal pitch, qreal distance);

private:
    QByteArray modelBytes_;
    std::unique_ptr<QTemporaryFile> modelFile_;
    qreal yaw_ = 25;
    qreal pitch_ = -15;
    qreal distance_ = 450;
};

class ModelViewCommand : public QUndoCommand
{
public:
    ModelViewCommand(ModelItem* model,
                     const QImage& before,
                     const QImage& after,
                     qreal oldYaw,
                     qreal oldPitch,
                     qreal oldDistance,
                     qreal newYaw,
                     qreal newPitch,
                     qreal newDistance);
    void redo() override;
    void undo() override;

private:
    ModelItem* model_;
    QImage before_;
    QImage after_;
    QRectF crop_;
    qreal oldYaw_, oldPitch_, oldDistance_;
    qreal newYaw_, newPitch_, newDistance_;
};

#endif

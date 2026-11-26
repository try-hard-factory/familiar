#include "modelitem.h"
#include <algorithm>
#include <cmath>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QPainter>

namespace {
QImage placeholder(const QString& filename)
{
    QImage image(400, 400, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#28313d"));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#afc5df"));
    painter.setFont(QFont(QStringLiteral("Sans Serif"), 18));
    painter.drawText(image.rect().adjusted(20, 20, -20, -20),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     QFileInfo(filename).fileName()
                         + QStringLiteral("\nSelect, then unlock to rotate"));
    return image;
}
} // namespace

ModelItem::ModelItem(const QImage& preview,
                     QByteArray modelBytes,
                     const QString& filename)
    : PixmapItem(preview.isNull() ? placeholder(filename) : preview, filename)
    , modelBytes_(std::move(modelBytes))
    , bounds_(model_bounds(modelBytes_))
{}

void ModelItem::set_prepared_assets(std::shared_ptr<QTemporaryDir> directory,
                                    const QUrl& url)
{
    preparedDir_ = std::move(directory);
    preparedUrl_ = url;
}

QVariantMap ModelItem::get_extra_save_data() const
{
    QVariantMap data = PixmapItem::get_extra_save_data();
    data[QStringLiteral("yaw")] = yaw_;
    data[QStringLiteral("pitch")] = pitch_;
    data[QStringLiteral("distance")] = distance_;
    data[QStringLiteral("previewVersion")] = kPreviewVersion;
    return data;
}

void ModelItem::apply_extra_save_data(const QVariantMap& data)
{
    PixmapItem::apply_extra_save_data(data);
    if (data.contains(QStringLiteral("opacity"))) {
        setOpacity(std::clamp(data.value(QStringLiteral("opacity")).toReal(),
                              0.0,
                              1.0));
    }
    set_grayscale(data.value(QStringLiteral("grayscale")).toBool());
    set_view(data.value(QStringLiteral("yaw"), 25).toReal(),
             data.value(QStringLiteral("pitch"), -15).toReal(),
             data.value(QStringLiteral("distance"), 450).toReal());
}

IBaseItem* ModelItem::create_copy()
{
    auto* copy = new ModelItem(pixmap().toImage(), modelBytes_, filename());
    copy->setPos(pos());
    copy->setZValue(zValue());
    copy->setScale(scale());
    copy->setRotation(rotation());
    copy->apply_extra_save_data(get_extra_save_data());
    if (flip() == -1) {
        copy->do_flip(false, {});
    }
    copy->set_encoded_source(encoded_source(), encoded_source_format());
    return copy;
}

void ModelItem::set_view(qreal yaw, qreal pitch, qreal distance)
{
    yaw_ = std::isfinite(yaw) ? yaw : 25;
    pitch_ = std::isfinite(pitch) ? std::clamp(pitch, -89.0, 89.0) : -15;
    distance_ = std::isfinite(distance) ? std::clamp(distance, 100.0, 2000.0)
                                        : 450;
}

QUrl ModelItem::model_url()
{
    if (modelBytes_.isEmpty()) {
        return {};
    }
    if (!modelFile_) {
        auto file = std::make_unique<QTemporaryFile>(
            QDir::tempPath() + QStringLiteral("/familiar-model-XXXXXX.glb"));
        if (!file->open() || file->write(modelBytes_) != modelBytes_.size()
            || !file->flush()) {
            return {};
        }
        modelFile_ = std::move(file);
    }
    return QUrl::fromLocalFile(modelFile_->fileName());
}

ModelViewCommand::ModelViewCommand(ModelItem* model,
                                   const QImage& before,
                                   const QImage& after,
                                   qreal oldYaw,
                                   qreal oldPitch,
                                   qreal oldDistance,
                                   qreal newYaw,
                                   qreal newPitch,
                                   qreal newDistance)
    : QUndoCommand(QObject::tr("Rotate 3D model"))
    , model_(model)
    , before_(before)
    , after_(after)
    , crop_(model->crop())
    , oldYaw_(oldYaw)
    , oldPitch_(oldPitch)
    , oldDistance_(oldDistance)
    , newYaw_(newYaw)
    , newPitch_(newPitch)
    , newDistance_(newDistance)
{}

void ModelViewCommand::redo()
{
    model_->set_view(newYaw_, newPitch_, newDistance_);
    model_->setPixmap(QPixmap::fromImage(after_));
    model_->set_crop(crop_);
}

void ModelViewCommand::undo()
{
    model_->set_view(oldYaw_, oldPitch_, oldDistance_);
    model_->setPixmap(QPixmap::fromImage(before_));
    model_->set_crop(crop_);
}

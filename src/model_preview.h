#ifndef MODEL_PREVIEW_H
#define MODEL_PREVIEW_H

#include <QByteArray>
#include <QImage>
#include <QVector3D>

struct ModelBounds
{
    QVector3D center;
    float extent = 1;
};

ModelBounds model_bounds(const QByteArray& glb);

// Software fallback when Qt Quick 3D cannot render a thumbnail.
QImage render_model_preview(const QByteArray& glb);

#endif

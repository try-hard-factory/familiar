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

// A quick software thumbnail. It samples the mesh vertices and its base
// colour texture, so importing a model does not need to start Qt Quick 3D.
QImage render_model_preview(const QByteArray& glb);

#endif

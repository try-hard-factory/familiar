#ifndef MODEL_PREVIEW_H
#define MODEL_PREVIEW_H

#include <QByteArray>
#include <QImage>

// A quick software thumbnail. It samples the mesh vertices and its base
// colour texture, so importing a model does not need to start Qt Quick 3D.
QImage render_model_preview(const QByteArray& glb);

#endif

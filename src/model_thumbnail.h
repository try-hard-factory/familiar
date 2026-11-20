#ifndef MODEL_THUMBNAIL_H
#define MODEL_THUMBNAIL_H

#include <QString>

// Runs in a separate Familiar process, before the single-instance guard.
// Returns zero only when the 3D renderer wrote a usable preview image.
int render_model_thumbnail_file(const QString& glbPath,
                                const QString& imagePath);

#endif

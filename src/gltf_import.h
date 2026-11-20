#ifndef GLTF_IMPORT_H
#define GLTF_IMPORT_H

#include <QByteArray>
#include <QString>

struct GltfImportResult
{
    QByteArray glb;
    QString error;
};

// Converts a JSON glTF and its local/data-URI resources into a self-contained
// GLB. The result can use the existing model viewer and .fml storage path.
GltfImportResult import_gltf(const QString& filename);

#endif

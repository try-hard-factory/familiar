#ifndef OBJ_IMPORT_H
#define OBJ_IMPORT_H

#include "gltf_import.h"

// Converts OBJ geometry and local MTL textures into the GLB used by ModelItem.
GltfImportResult import_obj(const QString& filename);

#endif

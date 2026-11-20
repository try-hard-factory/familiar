# 3D canvas prototype

This branch adds `.glb`, `.gltf`, and `.obj` models as canvas items. Build with
Qt 6 including Quick Widgets, Quick 3D, and Concurrent. In Familiar, choose
**Insert → 3D model...**, drag a local model onto the canvas, or paste a copied
model file.
For `.gltf`, keep its referenced buffers and images beside it. For `.obj`, keep
its `.mtl` and diffuse textures (`map_Kd`) beside it. Files in subfolders are
also supported. Imported resources are packed into a GLB, so the saved board
does not depend on the source files.
If the `.mtl` does not name textures, the OBJ importer also looks for image
files named after its materials in a `textures` folder beside the OBJ folder.
It recognizes base color, normal, roughness, metallic, occlusion, and emissive
maps. Large OBJ files are prepared in the background with a cancelable dialog.
The imported item is selected and transformed like a picture. Select it and
press the padlock button below it to unlock 3D rotation; double-clicking the
model is a shortcut. Drag inside its frame to rotate and use the wheel to
change the camera distance. Press the padlock again or **Esc** to return to
the canvas. The camera change is undoable.

The `.fml` contains the imported GLB, camera angles and distance, and a
preview image. Scene export uses the preview. The viewer creates a temporary
file while the item is alive because Qt's runtime loader reads from a URL.

This is a trial implementation. Live 3D controls use an axis-aligned widget,
so a model item with a 2D rotation or crop can have a mismatched editing frame.
The model preview is captured from the Qt Quick widget when editing ends; a
graphics backend that cannot capture a framebuffer keeps the previous preview.
Older Familiar versions do not understand the `model` item type.

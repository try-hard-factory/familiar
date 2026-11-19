# 3D canvas prototype

This branch adds a `.glb` model as a canvas item. Build with Qt 6 including
Quick Widgets and Quick 3D. In Familiar, choose **Insert → 3D model (.glb)...**,
drag a local `.glb` onto the canvas, or paste a copied `.glb` file.
The imported item behaves like a picture until you double-click it. Drag inside
its frame to rotate, use the wheel to change the camera distance, and press
**Esc** to return to the canvas. The camera change is undoable.

The `.fml` contains the original `.glb`, camera angles and distance, and a
preview image. Scene export uses the preview. The viewer creates a temporary
file while the item is alive because Qt's runtime loader reads from a URL.

This is a trial implementation. Live 3D controls use an axis-aligned widget,
so a model item with a 2D rotation or crop can have a mismatched editing frame.
The model preview is captured from the Qt Quick widget when editing ends; a
graphics backend that cannot capture a framebuffer keeps the previous preview.
Older Familiar versions do not understand the `model` item type.

# 3D canvas prototype

This branch adds a `.glb` model as a canvas item. Build with Qt 6 including
Quick Widgets and Quick 3D. In Familiar, choose **Insert → 3D model (.glb)...**,
drag a local `.glb` onto the canvas, or paste a copied `.glb` file.
The imported item is selected and transformed like a picture. Select it and
press the padlock button below it to unlock 3D rotation; double-clicking the
model is a shortcut. Drag inside its frame to rotate and use the wheel to
change the camera distance. Press the padlock again or **Esc** to return to
the canvas. The camera change is undoable.

The `.fml` contains the original `.glb`, camera angles and distance, and a
preview image. Scene export uses the preview. The viewer creates a temporary
file while the item is alive because Qt's runtime loader reads from a URL.

This is a trial implementation. Live 3D controls use an axis-aligned widget,
so a model item with a 2D rotation or crop can have a mismatched editing frame.
The model preview is captured from the Qt Quick widget when editing ends; a
graphics backend that cannot capture a framebuffer keeps the previous preview.
Older Familiar versions do not understand the `model` item type.

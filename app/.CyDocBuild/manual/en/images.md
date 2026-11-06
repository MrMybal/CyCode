---
id: images
title: View images
description: Zoom and pan read-only image tabs.
section: Work in CyCode
order: 50
parent: editing
revision: 2
---

## Open and navigate {#preview}

Open an image from Project, the file picker, search or drag-and-drop. Explicit file selection can grant read access to one image outside the workspace; it does not grant write access to that folder.

- Use the wheel to zoom around the pointer.
- Pan with the middle mouse button or Space+drag.
- Choose Fit, 100%, zoom controls or double-click to fit.
- Reload explicitly after the image changes. A failed reload retains the previous valid preview.

## Formats {#formats}

PNG, JPEG, BMP, TGA and static WebP are supported. GIF displays its first frame. PSD uses its flattened image and HDR is converted for display. SVG stays editable XML. TIFF, ICO, DDS, AVIF, EXR and animated WebP do not have a preview.

Image tabs do not provide painting, cropping or image export. Original image bytes remain unchanged. Text find, text saving and breakpoints do not apply to these tabs.

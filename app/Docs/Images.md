# Image previews

CyCode opens image files in read-only document tabs using **CyUI::Images** and
its automatically selected **Canvas** dependency. Tabs share the normal docking,
detachment, close/reopen and next/previous-document actions with code editors.
There is no image decoder or renderer copied from CyUI.

Open an image from Project, Go to file/Search Everywhere, File > Open image,
drag-and-drop, or `--open` within the active workspace. A positional image path
starts CyCode with that image's parent folder as its workspace.
Explicit file selection/drop also permits an image outside the workspace: only
that canonical file receives read access. This does not extend Engine document
permissions or permit writes outside the workspace. Reopening a tracked external
image uses the same selected source; reloading rejects a retargeted symlink.

The view provides:

- Wheel zoom around the pointer; middle-button or Space+drag panning.
- Fit image, actual pixels (100%), zoom buttons and double-click to fit.
- Theme-aware transparency checkerboard and optional nearest-neighbor sampling.
- Dimensions, displayed zoom, input file size and pixel coordinates under the pointer.
- Explicit reload, preserving the previous valid preview if decoding/upload fails.

## Formats and scope

| Format | Preview |
| --- | --- |
| PNG, JPEG/JFIF, BMP, TGA | RGBA preview; PNG transparency and progressive JPEG included |
| WebP | Static lossy/lossless images with transparency |
| GIF | First frame, with a visible first-frame label; animation playback remains pending |
| PSD | Flattened/composited image; no layer editing |
| Radiance HDR | Converted to 8-bit display using stb_image's default conversion; original bytes unchanged |
| Softimage PIC, binary PGM/PPM | Supported by stb_image |
| SVG | Remains editable XML; no raster preview yet |
| TIFF, ICO, DDS, AVIF, EXR, animated WebP | No preview yet; binary formats show an explicit unsupported message |

This is a viewer, without painting, cropping transactions, image exports, EXIF
orientation transforms or ICC color management. Code/language services, VCS,
Engine text documents and the MCP text API remain separate. Save, text editing,
document find and breakpoints are disabled for image tabs. File/source permissions
are checked before reading, including again on the decoder worker.

## Ownership, responsiveness and dependencies

The application owns binary input, decoding and OpenGL texture lifetimes. One
worker decodes off the UI thread; at most one completed RGBA result waits for
consumption. Reload/close cancels stale requests. GPU upload and deletion run on
the UI's existing shared OpenGL context. Texture deletion is deferred until the
next frame so detached platform viewports finish using the previous draw lists.
CPU pixel buffers are released after upload.

Inputs are bounded to 64 MiB, 32768 pixels per side and 64 x 1024 x 1024 pixels
(including 8192 x 8192 textures). GPU limits are checked before allocation, and
open previews have a 512 MiB texture budget. Oversized/invalid files report an
error instead of being passed to the code editor.

`app/dependencies.lock.json` pins stb and libwebp by full commit and archive SHA-256.
Only `webpdecoder` is linked; encoders, demux/mux tools and viewers are not built.
Both decoders are desktop-only dependencies: `--core-only` does not fetch or build
them. MIT/Unlicense (stb) and libwebp's BSD/patent/authorship notices are preserved
under `app/licenses`. No additional decoder DLL or Python package is needed at run time.

## Validation

The `images` CTest uses real PNG/JPEG/WebP/GIF/BMP/TGA/PSD/HDR/PGM/PPM fixtures,
Unicode paths, malformed/oversized inputs, cancellation and actual app views with
headless ImGui frames. Its injected renderer verifies RGBA handoff, draw-list
texture IDs, delayed deletion, reload failures, zoom/pan, image/text command routing
and workspace/external-file permissions. The rendering checks cover six themes,
English/French and 100/150/200% DPI.

Native OpenGL upload/display and detached multi-monitor image windows have not
been run in this session, to avoid reopening the desktop app. The renderer adapter
is compiled for Windows and its resource contract is exercised headlessly; this
is not evidence of GPU screenshot acceptance. Linux/macOS remain unexecuted.

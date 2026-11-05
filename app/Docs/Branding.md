# CyCode branding

The owner-supplied assets live in the repository's `icons/` directory:

- `CyCode.png`: transparent application logo, used in the top bar and SDL window icons.
- `CyCode.ico`: Windows executable application icon, resource ID 1 / icon index 0.
- `CyCode-document.ico`: document icon, resource ID 2 / icon index 1.
- `CyCode-document.png`: original document artwork, included in distribution.

Each ICO contains 16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 px images. CMake
configures the Windows resource paths from the repository root; source code has
no machine-specific path. Only the GUI executable embeds these icon resources.
The UI-free CLI keeps its existing dependencies.

The desktop reads the PNG from its installation, decodes it with the existing
native image decoder and prepares a display copy of at most 256 px. The original
assets stay unchanged. The top bar uses the existing OpenGL texture owner and
CyUI dimensions. The image retains its supplied colours in each theme.

SDL's Windows class-icon hints select resource 1 for large/small icons before
initialization. The cached image is also applied when SDL shows a main or detached
window. Its pixel/texture lifetime belongs to Application, including deferred GPU
deletion. Missing/broken artwork cannot prevent project editing; the text title
and embedded Windows resource remain available.

Windows packaging explicitly includes the four PNG/ICO files and records their
hashes. Keep `icons/`, `app/` and `tools/` alongside `CyCode.exe`. The document icon
is available to a future installer/file-association workflow; adding it does not
register file extensions or change system defaults.

Windows x64 resource embedding and native icon/logo rendering have been verified.
Linux/macOS runtime and native desktop/bundle packaging remain unverified; a
Wayland desktop entry or a macOS application bundle is still separate work.

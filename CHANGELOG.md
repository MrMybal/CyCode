# Changelog

## Unreleased

- License CyCode's original source under GNU GPL v3 only (`GPL-3.0-only`), as
  selected by the owner. Include the complete license and copyright notice in
  Windows archives, and update EN/FR UI and documentation. Dependency licenses
  remain unchanged. CyCodeReader and its shared editor source use GPL-3.0-only too.

## 0.2.0 — 2026-11-03

- Migrate IDE views to explicit CyUI modules: Code/CodeEditor, Diff, Commands,
  Diagnostics, DocumentViews, Inspector and Images, plus their Core/Canvas dependencies.
- Share the editor/syntax implementation with the independent CyCodeReader repository;
  pin both source dependencies to exact commits.
- Add Rust, Java, JavaScript/TypeScript and HTML/CSS services, richer completion,
  symbol documentation, guarded code actions and Cargo/npm/standalone Java tasks.
- Add asynchronous read-only image tabs with zoom, pan, transparency and reload.
- Integrate common filtering, localization, themes and DPI handling in migrated views.
- Include the official CyUI MIT license link and local third-party notices.
- Provide a Windows x64 development archive with IDE, CLI and pinned ripgrep.

Windows/MSVC native and headless UI tests cover this milestone. Native GPU image
display and clean-machine installation have not been validated. Linux/macOS execution,
complete Rider parity and a distributable MCP/language-toolchain bundle remain pending.
The optional MCP runtime requires the actual CyToolsCore wheel and local setup.
At the time of this release, CyCode's own license had not yet been selected.

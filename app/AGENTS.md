# CyCode development

Read README.md, Docs/Architecture.md, Docs/CapabilityCoverage.md and Docs/Validation.md.
This is a native C++20/CyUI IDE with a private CyToolsCore MCP companion.

- Preserve the common native services used by the GUI and MCP; do not add an independent Python editor model.
- Use the configured CyUI checkout. Read its docs/AI_GUIDE.md. Palette roles for colours, cyui::dp/style for dimensions. Do not edit its vendored ImGui.
- Keep the native core free of SDL/ImGui. Keep plugin contracts in the versioned C ABI; no STL or ImGui objects across that boundary.
- English by default, French selectable. Add source labels to scripts/write_locales.py and regenerate locales/ui.json.
- Workspace document references are persistent project files, not temporary CyTools task artifacts. Validate paths, revisions and disk conflicts before editing or saving.
- Use CyToolsCore's real schemas/runtime/scheduler/identities/MCP. Do not reimplement them. Read the SDK Docs/AgentGuide.md, MCP.md, Protocol.md and TaskFiles.md before changing the bridge.
- Spawn external tools with argv, never a shell command assembled from project data. Keep process output bounded and cancellation tied to actual process termination.
- Never publish credentials, runtime/, data/, downloaded dependencies or a project license that has not been selected by the owner.
- Build: python app/scripts/build.py from the project root. Run native CTest, Python integration tests and GUI capture checks after relevant changes.
- Windows is locally validated. Linux/macOS remain unverified until those builds actually run. Schemas and CI configuration do not establish support.
- Keep remaining language backends, DAP, web/shader previews and complete VCS functionality explicit in Docs/Roadmap.md. A descriptor does not implement a feature.

The generated generic Python desktop/model forms have been replaced by the native IDE.
There is no model/CUDA requirement for editing, compilation or the MCP bridge.

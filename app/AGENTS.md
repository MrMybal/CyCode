# CyCode Core development

Read README.md, Docs/Architecture.md, Docs/CapabilityCoverage.md and Docs/Validation.md.
This repository provides the native C++20 Core and CLI/service. UI sources live in CyCodeIDE.
Read Docs/CoreExtraction.md before changing the build or shared APIs.

- Preserve the common native services used by the GUI and MCP; do not add an independent Python editor model.
- Use the configured CyUI checkout. Read its docs/AI_GUIDE.md. Palette roles for colours, cyui::dp/style for dimensions. Do not edit its vendored ImGui.
- Keep the native core free of SDL/ImGui. Keep plugin contracts in the versioned C ABI; no STL or ImGui objects across that boundary.
- English by default, French selectable. Add source labels to scripts/write_locales.py and regenerate locales/ui.json.
- Workspace document references are persistent project files, not temporary CyTools task artifacts. Validate paths, revisions and disk conflicts before editing or saving.
- Use CyToolsCore's real schemas/runtime/scheduler/identities/MCP. Do not reimplement them. Read the SDK Docs/AgentGuide.md, MCP.md, Protocol.md and TaskFiles.md before changing the bridge.
- Spawn external tools with argv, never a shell command assembled from project data. Keep process output bounded and cancellation tied to actual process termination.
- Never publish credentials, runtime/, data/, downloaded dependencies or a project license that has not been selected by the owner.
- Routine local validation: python app/scripts/verify_sources.py (syntax/data only, no native executables). Build: python app/scripts/build.py from the project root; ordinary builds must keep BUILD_TESTING=OFF and must not run tests.
- Native CTest requires the explicit --run-tests build option and a separate test build. MCP, debugger, generated-project and GUI acceptance suites also launch native processes; do not run them implicitly or relaunch the visible app. See Docs/Testing.md.
- If antivirus removes a test binary, stop native runs; do not automatically rebuild/retry, restore quarantined files or change antivirus settings. While the current Kaspersky removals remain unexplained, use source checks and app-only builds and record unexecuted native checks honestly.
- Windows is locally validated. Linux/macOS remain unverified until those builds actually run. Schemas and CI configuration do not establish support.
- Keep remaining language backends, DAP, web/shader previews and complete VCS functionality explicit in Docs/Roadmap.md. A descriptor does not implement a feature.

The generated generic Python desktop/model forms have been replaced by the native IDE.
There is no model/CUDA requirement for editing, compilation or the MCP bridge.

# CyCode Core development

Read README.md, Docs/Architecture.md, Docs/CapabilityCoverage.md and Docs/Validation.md.
This repository provides the native C++20 Core and CLI/service. UI sources live in CyCodeIDE.
Read Docs/CoreExtraction.md before changing the build or shared APIs.

- Preserve the common native services used by the GUI and MCP; do not add an independent Python editor model.
- Keep native components independent of CyUI/SDL/ImGui. Graphical adapters, palettes and widget patches belong to CyCodeIDE / CyCodeReader.
- Keep the native core free of SDL/ImGui. Keep plugin contracts in the versioned C ABI; no STL or ImGui objects across that boundary.
- UI translations are owned by CyCodeIDE app/scripts/write_locales.py and CyCodeReader scripts/write_locales.py. The legacy Core entry requires an explicit --ide-root and does not maintain another UI catalogue.
- Workspace document references are persistent project files, not temporary CyTools task artifacts. Validate paths, revisions and disk conflicts before editing or saving.
- Use CyToolsCore's real schemas/runtime/scheduler/identities/MCP. Do not reimplement them. Read the SDK Docs/AgentGuide.md, MCP.md, Protocol.md and TaskFiles.md before changing the bridge.
- Spawn external tools with argv, never a shell command assembled from project data. Keep process output bounded and cancellation tied to actual process termination.
- Never publish credentials, runtime/, data/, downloaded dependencies or a project license that has not been selected by the owner.
- Routine local validation: python app/scripts/verify_sources.py (syntax/data only, no native executables). Build: python app/scripts/build.py from the project root; ordinary builds must keep BUILD_TESTING=OFF and must not run tests.
- Native CTest requires the explicit --run-tests build option and a separate test build. MCP, debugger, generated-project and GUI acceptance suites also launch native processes; do not run them implicitly or relaunch the visible app. See Docs/Testing.md.
- If antivirus removes a test binary, stop native runs; do not automatically rebuild/retry, restore quarantined files or change antivirus settings. While the current Kaspersky removals remain unexplained, use source checks and app-only builds and record unexecuted native checks honestly.
- Core d0b1791 passed the real native suite on Windows/Linux/macOS runners.
  Reader c4a013b passed its seven native integration cases on those systems.
  Production Windows compilation is local evidence; native desktop execution
  remains suspended locally. See Docs/CoreExtraction.md for precise CI provenance.
- Keep remaining language backends, DAP, web/shader previews and complete VCS functionality explicit in Docs/Roadmap.md. A descriptor does not implement a feature.

The generated generic Python desktop/model forms have been replaced by the native IDE.
There is no model/CUDA requirement for editing, compilation or the MCP bridge.

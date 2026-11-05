# Solutions and Unreal Engine

The reviewed cook/package workflow now has typed maps/container controls,
per-attempt staging/archive/logs and shared cancellable native/UI/SDK/ABI2
execution. Actual Unreal acceptance remains pending during the Kaspersky
suspension; see [Unreal automation](UnrealAutomation.md).

Use File > Open solution (Ctrl+O) to select a .sln or .uproject. A path can also
be passed as the first argument, with --workspace, or dropped onto the application.
File > New window launches an independent process; Open solution in
new window selects another project without switching the current workspace.

Opening a solution inspects project declarations, solution folders, solution items,
configurations and nested hierarchy. UTF-8/BOM and UTF-16 solutions are accepted.
Missing generated projects are marked; the workspace file tree provides the actual
sources. Solution inspection does not evaluate MSBuild. For an Unreal project,
normal desktop opening prepares its code analysis automatically as described below.
External references have a context menu to open a separate instance; they do
not widen the current workspace's document/MCP permissions. General Visual Studio
solutions can be inspected, edited and built through the shared
[MSBuild adapter](MSBuild.md).

An Unreal .uproject opens directly, including when no .sln exists. A matching
.uproject beside a selected .sln is detected. A directory containing one Unreal
project can also be opened; ambiguous projects require selecting the intended
.uproject. Target.cs, module Build.cs rules, local .uplugin descriptors and declared
modules/plugins appear in the Unreal Engine panel.

The engine is found using CYCODE_UNREAL_ENGINE, registered EngineAssociation
installations, or engine references in the solution. The panel also accepts an
explicit engine directory. Detection reads Build.version without installing or
downloading an engine. Select a matching engine with a built UnrealBuildTool, its
.NET runtime and the target platform SDK/toolchain.

## Native commands

All operations share native services between the UI, CLI and CyToolsCore/MCP:

| Operation | Action |
|---|---|
| unreal.inspect / unreal_inspect | Read project, engine, targets, modules, plugins and analysis status |
| unreal.generate / unreal_generate | Generate project files with UBT -ProjectFiles -Game; refresh the solution tree |
| unreal.database / unreal_database | Run UBT -Mode=GenerateClangDatabase -ExecCodeGenActions into a managed analysis directory |
| unreal.prepare / unreal_prepare | Prepare UHT/analysis automatically, or reuse a matching validated cache |
| unreal.build / unreal_build | Build the selected target/platform/configuration with UBT |
| unreal.automationPlan / unreal_automation_plan | Preview one typed UE5 cook/package action and managed output context |
| unreal.automationRun / unreal_automation_run | Execute its issued preview once through the shared cancellable build owner |

Native names use dots; MCP names are prefixed with op_ and use underscore IDs.
Inputs are project, engineRoot, target, platform and configuration. Inspect needs
no process permission. Generation, preparation, indexing and compilation execute trusted Unreal
C# rules and require process.execute in MCP.

UBT receives separate argv entries and runs from Engine/Source. The bundled .NET
runtime is preferred. No shell command is assembled from project names. A discovered
Editor target and host platform are selected by default. Configurations are Debug,
DebugGame, Development, Shipping and Test; platform choices require the corresponding
Unreal SDK. Output is bounded to 8 MiB and operations currently have a 180-second
default deadline. The native build service supports explicit cancellation and
1..86400-second deadlines; see [Build cancellation](BuildCancellation.md).
Automation has a separately reviewed 3600-second default. Streaming remains
future work.

Successful operations persist .cycode/unreal.json. Project generation writes the
usual Unreal Intermediate/solution files. Analysis preparation runs UHT/code generators
without a full C++ build and writes compile_commands.json under
.cycode/unreal-analysis/TARGET/PLATFORM/CONFIGURATION. It preserves an existing root
database and starts/reloads shared clangd with the selected database. A matching cache
is reused after checking rule/header inputs, source paths, generated headers and response
files. Cache misses force UHT header generation so individually missing headers are
recreated despite otherwise current UBT timestamps. UBT also owns its normal
engine/user caches and logs. Inspection and capture mode
remain read-only; normal Unreal desktop opening performs preparation in the background.

Correct Unreal analysis requires engine include paths/macros and generated UHT
headers. Preparation supplies these automatically; failures are reported in Build,
with a retry in the Unreal panel. Unreal macros/types also have a small local completion
catalogue while analysis is starting. Engine members and project symbols come from clangd.
Assets are listed but are not text editors. Blueprint analysis, Unreal Editor
automation, Live Coding, richer cooking/packaging, Unreal debugging and asset-aware
refactoring are pending. C++ definition/reference navigation can open registered
Engine/Source and Engine/Plugins sources in read-only tabs. Semantic C++ rename
previews references across project files; it does not update serialized Unreal names
or Blueprint assets. C# rules have syntax colouring, not a
Roslyn project service.

## Validation

Windows x64 / installed Unreal 5.3 was executed: actual project-file generation,
Editor-target compilation to a module DLL, UHT generation for a USTRUCT/UPROPERTY,
UBT compile_commands.json and a clangd definition into the struct's header.
The user's CyTrace_Plugin .sln/.uproject was inspected/rendered read-only; compilation
used a separate CyCode-owned project. Other engine versions and Linux/macOS execution
remain unverified.

Use the private Python runtime:

```text
python app/scripts/verify_unreal.py --engine ABSOLUTE_ENGINE_ROOT
python app/scripts/verify_unreal_completion.py --engine ABSOLUTE_ENGINE_ROOT
python app/scripts/verify_unreal_navigation.py
python app/scripts/verify_windows.py
```

The first script writes its owned fixture under data/validation, apart from normal
UBT caches/logs. It never builds the user's existing project. Reports are under
data/reports. Native tests cover solution hierarchy/configurations, missing/external
projects, UTF-16, cycles and workspace confinement. MCP tests verify discovery and
rejection of process actions by a read-only client.
After generating the owned completion fixture, verify_unreal_navigation.py checks
that AActor opens Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h as a
read-only SDK tab, leaving the fixture's source on disk unchanged.

References: [Epic project file generation](https://dev.epicgames.com/documentation/unreal-engine/how-to-generate-unreal-engine-project-files-for-your-ide),
[Epic code/project management](https://dev.epicgames.com/documentation/unreal-engine/managing-game-code-in-unreal-engine),
[Microsoft solution format](https://learn.microsoft.com/en-us/visualstudio/extensibility/internals/solution-dot-sln-file).

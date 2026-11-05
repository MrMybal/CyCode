# XML solutions and CMake Presets

## Visual Studio .slnx

The chooser, workspace/tree, separate-window opening, MSBuild adapter and C#
solution selection accept `.slnx` alongside `.sln`. Unreal discovery prefers `.sln`,
then checks `.slnx` when absent. Several solutions require explicit selection.
The UI-free reader uses pinned TinyXML2; opening does not evaluate imports, execute
generators, require .NET or rewrite the solution.

It reads UTF-8/BOM and the existing UTF-16 decoding, logical `/Folder/` paths with
implicit parents, files/projects, display names, explicit GUIDs, stable host IDs,
dependency references and missing/external paths. Configuration/platform products
default to Debug/Release and Any CPU. Managed/native platform defaults, ordered
BuildType/Platform/Build/Deploy rules, custom types/BasedOn and solution defaults
feed the existing MSBuild model. The actual build engine receives the original
solution; this reader does not replace its evaluator. Indexing retains its mapped
configurations and permission/bounds checks. C# acceptance also depends on the
installed language server's workspace support.

Bounds: 16 MiB input, 100,000 XML nodes, 32 XML/64 logical-folder levels, 10,000
items, 256 solution configurations, 200,000 project mappings and 64 custom types.
DTD/unknown declarations, non-whitespace text, duplicate IDs/paths/configurations,
applied inheritance cycles and malformed XML are rejected. Properties remain
owned by the actual build engine. Niche project types/aliases need further
real-project validation. Microsoft documents MSBuild 17.12+ for `.slnx`.
References: [MSBuild command reference](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference),
[official solution schema/model](https://github.com/microsoft/vs-solutionpersistence/tree/ea32bc0df5e54009a289d864bb720be9794a07d7).

## CMake Presets

The CMake panel selects Configure preset and an optional linked Build preset;
Manual settings retains the existing adapter. Presets own binary directory,
generator/toolchain and any fixed configuration. A target selection overrides
the preset target set. Default build uses the configured tree; choose a build
preset when its environment/jobs/targets are needed during compilation.

The bounded native reader handles CMakePresets.json/CMakeUserPresets.json versions
1–10, workspace-local include DAGs, implicit project inclusion by user presets,
inheritance with earlier-parent precedence, environment/cache maps and null values,
standard path/env macros and the defining file origin for `${fileDir}`. Hidden
presets serve as bases and are excluded from selectors. Entry errors remain
explicit; malformed preset files do not disable manual project inspection.

Execution uses `cmake --preset=NAME -S SOURCE -B BINARY` or
`cmake --build --preset=NAME`, with SOURCE as cwd. CMake applies cache variables,
conditions, toolchains, environment, targets, jobs and execution options. CyCode
adds compilation database/File API requests and a guarded binary directory.
The installed CMake must support the chosen schema. Conditional availability is
validated during execution; the metadata selector does not filter every condition.

Includes/binary paths must stay in the workspace after canonical resolution.
A selectable configure preset needs binaryDir; vendor macros cannot determine
host-owned build paths. In-source builds are refused. Snapshots are checked before
and after successful execution; changes prevent profile/analysis publication.
Native cancellation, deadlines and output bounds apply. Project scripts/toolchains
remain trusted executable code. MSVC/Ninja presets with external environment setup
still need a suitable developer environment.

Read-only `build.presets` / MCP `build_presets` returns entries, files and errors
without executing CMake. `build.inspect` includes the catalogue.
`build.configure`/`build.run` accept `configurePreset`/`buildPreset`, persisted in the
successful local profile. Older manual profiles still load. Plugins export the
same services; Python does not maintain a second model.

Bounds: 64 files, 2 MiB each/8 MiB total, 64 JSON/inheritance levels, 512 entries per
family, 256 KiB per resolved preset/16 MiB total and 32 KiB path expansion.
Test/package/workflow presets now have a shared preview and native execution path
in View > CMake preset actions, with revision guards, cancellation and the existing
build reservation. New runtime/GUI acceptance remains pending. Configured CTest
retains its separate structured workflow; see [Preset actions](CMakePresetActions.md).
Public catalogue metadata is bounded to 16 MiB and snapshots include root-file
presence. [CMake specification](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html).

## Validation

New C++ assertions cover XML hierarchies/mappings/defaults, MSBuild inspection and
invalid input; preset assertions cover includes/inheritance, user definitions,
path/env expansion, hidden entries, changed snapshots, cycles and escaped paths.
MSVC `/Zs` checks sources without generating test executables. Native execution
and graphical acceptance remain suspended following the Kaspersky removals.
Windows application compilation is recorded in [Validation](Validation.md);
Linux/macOS runtime behavior remains unverified.

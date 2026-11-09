# Visual Studio and MSBuild projects

The native XML solution reader shares the existing model, build/evaluation/index
services and guarded source access. See [Project formats](ProjectFormats.md) for
configuration rules, tool prerequisites and pending execution validation.

Open a .sln, .slnx, .vcxproj, .csproj, .fsproj or .vbproj directly. Non-Unreal solutions use
the native MSBuild service; Unreal continues to use UnrealBuildTool. A directory
with CMakeLists.txt keeps its CMake workflow, while opening its generated .sln
explicitly selects MSBuild. The native XML reader also supports .slnx; see ProjectFormats.md.

The MSBuild panel selects the solution configuration/platform, whole solution or
one local project, and Visual Studio MSBuild or dotnet msbuild. It exposes build,
rebuild, clean, package restore, explicit project evaluation and C++ code analysis.
The existing Build project shortcut routes to this adapter. Rider Ctrl+F9 and the
Visual Studio/VS Code build bindings execute the selected solution profile.
All operations use the same native service from the GUI, CLI and CyToolsCore MCP.
GUI build/rebuild saves modified document buffers first using normal revision/disk
guards, and refuses to compile when a buffer cannot be saved. Native/MCP builds use
the disk project; clients explicitly save their document edits before building.

Solution inspection parses ActiveCfg, Build.0 and Deploy.0, including nested folders,
without loading MSBuild imports. Selecting a project builds its actual solution
target, preserving MSBuild's dependency and configuration mapping behavior. Build
uses the project target name without a :Build suffix; clean/rebuild use their named
targets. Dotted project names use MSBuild's target-name escaping. Explicit package
restore on a selected project uses its mapped configuration and project file.

MSBuild/dotnet tools are discovered independently of the launching shell. C++ builds
require Visual Studio MSBuild on Windows. The .NET SDK backend builds managed
projects using installed SDKs and their normal package/import configuration. These
are trusted-project operations: targets, imports, property functions, packages and
build events can execute code and write outside the source root according to their
own project rules. Inspection does not evaluate those imports. Evaluation is an
explicit process.execute operation, including evaluation with no build targets.

The last successful profile is saved atomically in .cycode/msbuild.json. Opening
another explicit solution/project in the same directory preserves that entry's
identity. Failed builds do not save a different selection. Artifact paths are read
from actual evaluated TargetPath properties and can populate the native debugger
profile for C++ executables. Managed compilation does not provide C# language
services or a managed debugger.

Prepare code analysis uses MSBuild 17.8+ structured property/item evaluation. For
enabled local .vcxproj files it derives compile_commands.json from actual ClCompile
items, include paths, preprocessor definitions, forced includes, language standard
compile-as mode and evaluated exception handling (Sync, Async, SyncCThrow). It skips ExcludedFromBuild items. Generated analysis metadata now has a format revision; older caches require explicit preparation again. Analysis is capped at eight
projects, 20,000 translation units and 32 MiB per operation. Select a project to
prepare a separate index. AdditionalOptions produce review warnings rather than
being silently treated as complete compiler flags. Advanced PCH/modules/custom
toolchain cases require further work. Generated headers must already exist; this
evaluation step does not compile them.

The shared clangd service reloads the prepared database. Cache inputs include the
solution, local project files and existing local imports reported by MSBuildAllProjects;
changed/deleted inputs invalidate cached artifacts/index metadata. New imports and
SDK/tool updates may still require explicit preparation. Metadata and databases stay
under .cycode/msbuild-cache rather than overwriting user compile_commands.json.

MSVC/C# file diagnostics with source coordinates are parsed, deduplicated and shown
in Problems, with navigation restricted to workspace files. Directory identity checks
handle MSVC expanding Windows short temp paths. General MSBuild/global errors remain
available in Build output. Builds default to 150 seconds in the native API and accept
timeoutSeconds overrides; the desktop remembers its own configurable build limit.
Evaluation remains bounded to 15 seconds per project. Shared native cancellation
is implemented; streaming remains pending. Process results expose exitCode/timedOut/truncated; callers must inspect
them, since a completed MCP job can contain compiler errors.

The Windows native test creates a real two-project CMake-generated solution with a
nested dotted project, builds/executes C++, exercises clean/rebuild and a changed
Debug-to-Release mapping, prepares clangd navigation and checks actual compile errors.
It also builds/executes an SDK C# fixture using installed .NET 8 packs and no NuGet
network sources. A separate official MCP test verifies managed build/restore/evaluation
and process permissions. Run app/scripts/verify_msbuild.py for the graphical workflow.
Linux/macOS execution and F#/VB project builds remain unverified.

Primary references: [MSBuild property/item evaluation](https://learn.microsoft.com/en-us/visualstudio/msbuild/evaluate-items-and-properties),
[solution project targets](https://learn.microsoft.com/en-us/visualstudio/msbuild/how-to-build-specific-targets-in-solutions-by-using-msbuild-exe?view=visualstudio),
[command-line reference](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference).

The CL metadata mappings follow [Microsoft’s CL task reference](https://learn.microsoft.com/en-us/visualstudio/msbuild/cl-task?view=vs-2022#parameters). AdditionalOptions still require explicit review.

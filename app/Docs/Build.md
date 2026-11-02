# C/C++ toolchains and CMake

Visual Studio solution/project builds have a separate native [MSBuild adapter](MSBuild.md).
Unreal solutions retain their UnrealBuildTool workflow.

Open Build > CMake settings, or start the desktop with --show-cmake. The CMake
panel selects source/build directories, toolchain, generator, configuration and
target. Configure applies those settings; Build compiles the selected target, or
the default target set when All targets is selected. Output remains in Build.

Detection runs off the UI thread. It queries CMake capabilities, compiler versions
and installed Visual Studio instances through vswhere. Standard Clang/GCC pairs
are found on PATH; Windows also checks LLVM and CMake/Ninja bundled with Visual
Studio. A generator listed by CMake is a supported generator format, not proof that
every prerequisite for that generator is installed. GCC is detected by the same
portable path but was not installed or exercised on the local Windows machine.
The initial adapter offers installed Visual Studio generators, Ninja (single
configuration), and Unix Makefiles when their build tools are found. Ninja Multi-Config,
Xcode and other generator integrations remain future work.

Automatic selects a discovered compiler for a new tree and preserves the compiler
and generator of an existing tree. On Windows, MSVC uses a Visual Studio generator,
x64 host/target and the detected MSVC tools version. This milestone does not use
MSVC with Ninja: localized showIncludes/dependency handling needs separate validation.
Clang/GCC use Ninja when available, otherwise Unix Makefiles. Custom toolchain files,
cross compilation, compiler wrappers and CMake Presets are future work.

A compiler or generator change requires a new build directory. CyCode does not
delete CMake caches. Existing trees without compiler metadata can be configured
with Automatic first, or replaced by an explicitly chosen new build directory.
Source and build paths must remain in the workspace; in-source builds and build
directories containing the source tree are rejected. Build configuration choices
are Debug, Release, RelWithDebInfo and MinSizeRel. A single-configuration generator
must be reconfigured before building a different configuration.

## Project metadata and code analysis

CyCode requests codemodel-v2 and toolchains-v1 using its own CMake File API client
query directory. It reads targets, artifacts and actual compiler information from
the referenced replies. It never evaluates CMake scripts to inspect a configured
project. Configure/build operations execute trusted project code as normal CMake
operations do. Inspect validates reply references and source/build identities.

For Ninja/Makefiles, the IDE uses CMake's native compile_commands.json. Visual
Studio generators do not export it, so CyCode derives a database from the selected
configuration's CMake File API compile groups: compiler, command fragments,
definitions, includes and reported implicit include directories. Only workspace
C/C++ translation units are exported. The generated database lives under
BUILD/.cycode/clangd/CONFIGURATION and is labelled cmake-file-api in the UI/API.
It describes analysis commands rather than a replacement build system. CMake still
owns the actual build. Advanced module/PCH/unity/framework and cross compilation
workflows have not been validated by this adapter.

The shared clangd receives the selected database directory through its initialization
options. A successful configure restarts it and resynchronizes live unsaved buffers,
including when flags change at the same database path. CyCode does not copy a database
over the workspace's root compile_commands.json or edit the user's .clangd.
Failed configure operations keep the previously active profile/database selection.

Successful operations save the active local profile in .cycode/build.json, schema
version 1. It contains source, buildDirectory, configuration, toolchain, generator
and target. Toolchain IDs may contain machine paths; this is local state, ignored
in CyCode's own repository. Invalid profiles are reported and preserved. They can
be repaired or removed explicitly; CyCode will not overwrite an unrecognized format.
The build tree gets a .gitignore if none exists, and .cycode metadata is excluded
from project text/file search. Existing ignore files are preserved.

Primary references: [CMake File API](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html),
[compilation database export](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html),
[clangd compilation settings](https://clangd.llvm.org/extensions#compilation-commands).

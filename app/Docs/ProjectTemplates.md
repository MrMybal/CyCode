# Project creation

File → New project (also `file.newProject` in the command palette) opens the
native CyUI wizard. Choose a template, project name and an existing parent
directory. Preview shows the exact UTF-8 files, syntax highlighting and tool
requirements. Create and open publishes a new directory and opens its project
entry and first source file. The optional new-window checkbox launches a separate
instance only on that explicit action.

| Template ID | Generated entry | Native build adapter |
| --- | --- | --- |
| `cpp.console` | CMake directory, C++20 source and CTest smoke test | CMake configure/build |
| `c.console` | CMake directory, C17 source and CTest smoke test | CMake configure/build |
| `rust.console` | Cargo manifest, Rust 2021 source and unit test | Cargo configure/build/test/run |
| `csharp.console` | Visual Studio `.sln`, SDK `.csproj`, C# source | MSBuild restore/build/run |
| `java.console` | `src/main/java/Main.java` | Standalone javac/java build/run |
| `web.javascript` | HTML/CSS, ES modules, npm scripts and local server | npm build/test/run |
| `cycode.unreal.cppGame` (bundled plugin) | `.uproject`, runtime module, Game/Editor target rules | Existing Unreal generation/build/index services |

Active ABI2 plugins can contribute additional models and typed options to this
same wizard. The bundled Unreal extension provides the C++ game model. Select the
installed engine association (default `5.3`), then generate/build using the Unreal
view. Its name must start with a letter and use letters, digits or underscores.
Unreal Engine itself remains an external installation.

.NET targets are explicitly selectable: `net8.0` (default), `net9.0`, `net10.0`.
Install a compatible SDK and matching targeting/runtime packs. The wizard does
not run a tool installer, restore packages, execute project code, initialize Git
or choose a project license. Each project contains build/run instructions in its
README. No extra CyCode or CyUI dependency enters generated projects.

The web template uses only Node built-ins (Node 18+ for `node --test`). Build
checks JavaScript syntax; it is not a bundler. Run serves an explicit list of
assets on `127.0.0.1:3000`; pass another port to the start script if needed. It
does not launch a browser. No `npm install` or external package is necessary.
Java and .NET include a `--self-test` executable path, not a test framework;
their broader IDE test integrations remain pending. CMake tests run through
CTest in the generated build directory, as explained in its README.

## Native API, MCP and plugin access

`ProjectTemplates` belongs to the UI-free native core. Engine exports:

- `templates.list` / MCP `templates_list`: catalogue and target frameworks.
- `templates.preview` / MCP `templates_preview`: `{template, name,
  destination?, framework?, options?}` returns exact files/content, an issued plan ID,
  requirements, instructions, project entry and initial source file. No writes.
- `templates.create` / MCP `templates_create`: `{id}` applies that issued plan.
  Requires `workspace.write` in the actual CyToolsCore runtime and plugin host.

MCP/plugin destinations are **workspace-relative** and cannot expand that
workspace boundary. An omitted destination uses the project name. The destination
parent must already exist and the final directory must not exist. Desktop uses
the same native class with the parent explicitly selected in its wizard; it does
not extend the current Engine/MCP workspace or its document permissions.

Names start with an ASCII letter and contain up to 64 letters, digits, `_` or `-`.
Windows device names are rejected on every platform. Traversing/protected paths,
links and redirected directories are refused. Plans are immutable, expire after
ten minutes, and retain the newest 32 per instance. Unknown, expired, evicted or
already-created plans cannot be applied.

Catalogue entries include the supplying `plugin`, `fields`, `nameStyle` and a
registry revision. Preview returns resolved `options`, plugin ownership and its
`catalogueRevision`. Disabling/enabling a plugin or changing the registry invalidates
pending plugin plans; preview again. Creation holds the host's activation guard
through the filesystem commit. Preview does not invoke plugin callbacks. The GUI
refreshes the choices and removes a stale preview when plugin activation changes.

Creation checks the original root/parent directory identity, exclusively allocates
a sibling staging folder, writes files without replacement and verifies every
file against the preview. A same-volume exclusive rename publishes the folder:
Windows `MoveFileExW` without replacement, Linux `renameat2(RENAME_NOREPLACE)` or
macOS `renamex_np(RENAME_EXCL)`. Unsupported filesystems fail without an overwrite
fallback. Failure cleanup removes only matching generated files and empty staging
directories; unexpected changed content is retained. These checks are protection
against stale previews and accidental overwrites, not an OS sandbox for hostile
code racing filesystem operations.

The workspace switch uses existing document safeguards. If modified documents
prevent opening, creation still succeeds and the wizard retains the created
project and an Open created project button. Save/close the old documents, then
retry. Opening the source preserves normal editor Undo/Redo. Creation does not
offer a destructive project-wide Undo.

## Limits

Six models are built in; the Unreal C++ game model belongs to its bundled plugin.
Additional models use installed native ABI2 plugins, not arbitrary project-local
JSON files. Standalone data-pack installation, binary template assets, scripting,
conditional file sets, automatic SDK acquisition, CMake Presets and broader
framework/test models remain work. Creating a model does not implement a new
build-system adapter; use an existing adapter or an explicit project provider.

Windows x64 is the local validation platform. Linux/macOS implementations are
present but have not been built or executed locally. See [Validation](Validation.md).

## Writing an ABI2 project template

Register `host.contribute(context, "template", descriptor_json, nullptr)` during
initialization, using an ID under the plugin's namespace. No ABI structure/layout
changes or STL values cross the C boundary. The descriptor is copied, checked and
owned by the host. See the [Unreal example](../plugins/unreal/ProjectTemplate.h)
and [native SDK](Plugins.md).

```json
{
  "id": "example.tools.console",
  "title": "Example console",
  "language": "cpp",
  "buildSystem": "cmake",
  "nameStyle": "identifier",
  "requirements": ["CMake and C++ compiler"],
  "fields": [
    {"id":"standard", "title":"C++ standard", "type":"choice",
     "choices":["17","20"], "default":"20"}
  ],
  "entry": ".",
  "openFile": "src/main.cpp",
  "instructions": "Configure and build ${project.name} with CMake.",
  "files": [
    {"path":"CMakeLists.txt", "text":"cmake_minimum_required(VERSION 3.24)\nproject(${project.identifier} LANGUAGES CXX)\nadd_executable(${project.identifier} src/main.cpp)\nset_property(TARGET ${project.identifier} PROPERTY CXX_STANDARD ${option.standard})\n"},
    {"path":"src/main.cpp", "text":"int main() { return 0; }\n"}
  ]
}
```

Metadata requires `id/title/language/buildSystem/requirements`; files require
`path/text`. `entry` is a declared file/directory (`.` is the new project root),
and `openFile` must be a declared file, with exact spelling. `nameStyle` defaults
to `portable`; `identifier` additionally excludes hyphens. Optional `order` controls
the plugin catalogue order; templates do not use contextual `when` expressions.

Fields have stable `id/title/type/default`, optional `help`, and `choices` for
choice fields. Types: `boolean`, `choice`, `identifier` (ASCII letter followed by
letters/digits/underscores), or `value` (1–128 ASCII letters/digits/underscores,
dots, hyphens or braces; suitable for version/GUID values). Values are checked
both at registration and preview; unknown options and non-advertised choices fail.

Substitutions in file paths, text, entry and instructions are `${project.name}`,
`${project.identifier}` (hyphens become underscores), `${project.slug}` (lowercase),
`${project.upper}` (uppercase identifier), and `${option.ID}`. Booleans produce
`true/false`. Unknown tokens in these two namespaces fail; other `${...}` strings
remain literal, preserving CMake/JavaScript source. There is no script/expression
execution, callback, automatic license or repository initialization.

Bounds: 32 plugin templates / 1 MiB of registered descriptors; 256 KiB per
descriptor; 16 option fields / 32 choices per field; 1–64 UTF-8 text files,
64 KiB per source text, 256 KiB after expansion in total, 128 filesystem entries,
16 path levels. Paths reject absolute/root-relative/traversing/protected names,
Windows device names, file/directory collisions and ASCII case aliases. Platform
filesystem checks still protect exclusive creation for other name collisions.

---
id: project-templates
title: Create a project from a template
description: Review generated files, required tools and plugin-supplied options before creation.
section: Work in CyCode
parent: projects
order: 121
revision: 1
---

## Preview the new project {#create}

Choose **File > New project**, select a template, name and existing parent folder.
Preview shows the exact text files, entry, first source and required tools.
**Create and open** creates a new directory. The new-window checkbox opens a
separate instance only when explicitly chosen.

| Model | Project |
| --- | --- |
| C++ / C console | CMake, C++20 / C17 and a CTest smoke case |
| Rust console | Cargo, Rust 2021 and a unit test |
| C# console | `.sln` and SDK `.csproj`; choose net8.0, net9.0 or net10.0 |
| Java console | Standalone `javac` / `java` |
| JavaScript web | HTML/CSS, ES modules and npm scripts |
| Unreal C++ game | Bundled Unreal plugin; engine association and runtime/target rules |

Install compatible SDKs separately. Creation does not install tools, restore
packages, run generated code, initialise Git or choose a project licence.
The generated README explains building/running. The web model uses Node built-ins;
its explicit Run script serves the selected assets locally without opening a browser.
Java/.NET self-test examples are not a unified test-framework integration.

## Names, destinations and retries {#destinations}

The final directory must not exist; its parent must exist. Portable names begin
with a letter and use letters, digits, `_` or `-`, up to 64 characters. A model
requiring an identifier, including Unreal, excludes hyphens. Protected names,
traversal, redirected directories and links are refused.

Review again if the preview expires or its supplying plugin changes. Unknown or
already-used plans cannot create twice. Creation publishes verified generated
files without overwriting a destination. If creation succeeds but edited old
documents prevent switching workspaces, save/close them and use **Open created
project**. Do not create another copy merely to retry opening.

There is no destructive project-wide Undo. SDK destinations remain within their
current workspace; the GUI's selected parent does not expand MCP access.

## Extend the catalogue {#plugins}

Active ABI2 plugins can contribute declarative models with typed choices,
booleans, identifiers and values. File content uses `${project.name}`,
`${project.identifier}` and `${option.ID}` substitutions; it is not a script engine.
Models generate 1–64 UTF-8 text files, bounded to 256 KiB after expansion.
A new template does not implement another build-system adapter.
See [the plugin contract](doc:plugin-api#contributions) and the
[template operations](doc:api-templates#operations).

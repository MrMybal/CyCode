---
id: dotnet
title: C# and .NET projects
description: Load a solution, restore references and use Roslyn services.
section: Work in CyCode
parent: languages
order: 310
revision: 1
---

## Prepare the solution {#solution}

1. Open the `.sln` or the folder containing your `.csproj`.
2. Make the SDK requested by `global.json`, targeting packs and project dependencies available.
3. Use the MSBuild/restore controls for the selected solution configuration.
4. Open a `.cs` file and inspect the C# server in Problems. Wait for workspace loading to finish before querying symbols.

The analysis server is csharp-ls/Roslyn. Its private analysis SDK does not replace the project's own build SDK. `CYCODE_CSHARP_LS` and `CYCODE_CSHARP_DOTNET` can select absolute paths to the server and host. Project evaluation uses trusted MSBuild imports.

## Edit and debug {#services}

Completion, definition/references, structure, semantic colours, rename and formatting use the loaded Roslyn workspace. Formatting respects `.editorconfig`. Review rename changes before applying them, then save explicitly. Restart the server after restoring dependencies or changing SDK/project references; dynamic filesystem watching is not implemented.

Build a managed target with symbols, configure NetCoreDbg and choose its executable/arguments in Debugger. The [debugging guide](doc:debugging#launch) describes launch, stack and watches. This does not supply every Visual Studio .NET workflow.

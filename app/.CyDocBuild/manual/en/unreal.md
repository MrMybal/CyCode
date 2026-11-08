---
id: unreal
title: Unreal Engine projects
description: Open solutions, regenerate project data and prepare Unreal launches.
section: Work in CyCode
order: 140
parent: projects
revision: 3
---

## Select the engine {#engine}

Open the `.uproject` or its `.sln`. Check the detected engine root, version, target, platform and configuration in the Unreal view. Completion needs the correct engine headers and compile context; Unreal macros are not understood from ordinary C++ keywords alone.

## Generated project data {#generated}

Use Generate project files when generated solution entries are missing or outdated. Generate the compile database for clangd when needed. Select a real game/editor target before building through UnrealBuildTool. Project generation and builds execute the installed engine's trusted tools.

## C++ insertion snippets {#code-snippets}

The active bundled Unreal provider 1.6.0 adds four definitions to the shared snippet
palette: ue_log/uelog, ue_property, ue_function and ue_editor. UE_LOG offers editable
category/message and verbosity choices; the declarations expose Blueprint metadata.
ue_editor wraps selected text with WITH_EDITOR guards and retains literal layout.
Use Rider Ctrl+J to insert or Ctrl+Alt+J to surround a selection.

The entries use scope cpp and include the cycode.unreal owner label. They are
available in any C++ workspace while the plugin is active. They do not add includes,
provide an implementation, query reflection or verify Unreal compilation. Choose
a valid class/context and review the code. Disabling the provider revokes pending
proposals. See [plugin snippets](doc:refactoring#plugin-snippets).

## Launch and package {#launch}

Prepare an Editor, game or uncooked-server launch profile, then review its program, map, arguments and build configuration in the shared debugger. Desktop build-before-debug uses the bound target. Preparing a profile does not start Unreal.

UE5 cook/package actions offer an explicit preview, map list, configuration, container options and deadline. Review output/staging folders before execution. These actions do not deploy, sign or upload a game. Blueprint asset editing and a complete RiderLink-style integration are not provided.

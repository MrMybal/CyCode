---
id: unreal
title: Unreal Engine projects
description: Open solutions, regenerate project data and prepare Unreal launches.
section: Work in CyCode
order: 140
parent: projects
revision: 2
---

## Select the engine {#engine}

Open the `.uproject` or its `.sln`. Check the detected engine root, version, target, platform and configuration in the Unreal view. Completion needs the correct engine headers and compile context; Unreal macros are not understood from ordinary C++ keywords alone.

## Generated project data {#generated}

Use Generate project files when generated solution entries are missing or outdated. Generate the compile database for clangd when needed. Select a real game/editor target before building through UnrealBuildTool. Project generation and builds execute the installed engine's trusted tools.

## Launch and package {#launch}

Prepare an Editor, game or uncooked-server launch profile, then review its program, map, arguments and build configuration in the shared debugger. Desktop build-before-debug uses the bound target. Preparing a profile does not start Unreal.

UE5 cook/package actions offer an explicit preview, map list, configuration, container options and deadline. Review output/staging folders before execution. These actions do not deploy, sign or upload a game. Blueprint asset editing and a complete RiderLink-style integration are not provided.

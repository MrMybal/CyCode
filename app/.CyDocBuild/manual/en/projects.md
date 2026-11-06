---
id: projects
title: Build and run projects
description: Select a project adapter, target and configuration.
section: Work in CyCode
order: 120
revision: 2
---

## Select the project {#projects}

CyCode recognises CMake projects, Visual Studio solutions, Unreal projects, Cargo, npm, standalone Java, Maven and Gradle descriptors. Open the project root, then use the matching build view or Project tasks. Select the directory, configuration and target before running an action.

GUI tasks save edited workspace documents first. A cancelled save keeps the build from starting. Native SDK operations require saved, reconciled workspace documents before executing project tasks.

## Unreal projects {#unreal}

Unreal uses its engine, UnrealBuildTool and target rules. It does not become an ordinary CMake project when opened through a solution. Continue with [Unreal setup](doc:unreal#engine).

## A small C++ project {#example}

For a CMake workspace, configure it in the CMake view, inspect the actual target list, build a selected executable and set its debug profile. Configuration executes the trusted project's build scripts. See [CMake and MSBuild](doc:build#configure).

Build output and Problems link recognised compiler messages to source files. Cancellation asks the owned process tree to stop; it does not delete the project's outputs.

---
id: build
title: CMake and MSBuild
description: Configure toolchains and build an actual selected target.
section: Work in CyCode
order: 130
parent: projects
revision: 3
---

## Configure and build {#configure}

For CMake, choose the source and build directories, configuration, generator and compiler/toolchain. Configure once, then inspect the targets reported by CMake's File API. Use an existing configure/build preset where appropriate. Test, package and workflow presets have their own preview/run actions.

For a standard `.sln`, inspect the solution in MSBuild, select configuration/platform and choose Build, Rebuild, Clean or Restore. MSBuild and the relevant SDKs must be installed. An Unreal solution continues through [UnrealBuildTool](doc:unreal#generated).

## Keep the selected compiler {#toolchains}

CMake detection preserves driver names such as `clang++` and `g++`, including symbolic links. The C and C++ drivers can share one executable while selecting different link libraries. If an older saved toolchain is unavailable, refresh detection and select the installed compiler again. If CyCode reports that the compiler or generator changed, choose a new build directory and configure it; existing build files remain in place. Automatic settings and project presets keep their own compiler choices.

## Read output {#output}

The Build view follows the current native task and retains a bounded output tail. Problems extracts supported compiler locations. Choose Cancel to stop the current owned operation; already created build files remain. A finished process with a nonzero exit code is a build failure, even if its output was collected successfully.

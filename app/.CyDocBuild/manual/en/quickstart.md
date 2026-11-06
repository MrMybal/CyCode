---
id: quickstart
title: Open your first project
description: Open a folder, Visual Studio solution or Unreal project.
section: Start here
order: 10
parent: overview
revision: 2
---

## Open a project {#first-work}

1. Open CyCode. From File, open a workspace folder or choose the solution-opening action for a `.sln` file.
2. Select the project root, not a generated output directory. For Unreal, choose the `.uproject` or its solution.
3. Expand Project and open a source file. Generated solution entries and physical workspace files are distinct views.
4. Select the target and configuration before building. A project opening successfully does not install its compiler or engine.
5. Save an edit with Ctrl+S. Use [global search](doc:search#find) to locate related code.

For CMake, Cargo, npm, Maven and Gradle, continue with [project tools](doc:projects#projects). Missing generated Unreal project entries can be regenerated through the [Unreal tools](doc:unreal#generated).

## Close a workspace {#close}

Read Save / Discard / Cancel prompts before closing. Cancel keeps the session open. Closing an unsaved tab is different from removing a file from disk. Open a second application instance to keep another project available.

---
id: debugging
title: Run and debug
description: Prepare a launch profile, breakpoints, stack and variable inspection.
section: Work in CyCode
order: 190
parent: projects
revision: 2
---

## Start a session {#launch}

Choose a compatible debug adapter and configure the executable, working directory, arguments and environment. CodeLLDB serves native debugging; NetCoreDbg serves .NET; Java uses the JDT integration. Build the project and keep sources aligned with its binaries and symbols.

Set a source breakpoint in the gutter, then Run/Debug or explicitly Attach. Inspect the adapter's verified breakpoint status. A requested breakpoint is not necessarily active in the target.

## Inspect and continue {#inspection}

While paused, select a thread and frame to inspect scopes, variables and watches. Memory, disassembly, exception details, function/data/instruction breakpoints and reverse execution depend on the adapter's advertised capabilities.

Run to cursor temporarily resumes toward a source location. Set next statement changes the instruction pointer and can skip code; it is a different explicit action. Variable/memory edits affect the debugged process and cannot be undone with document Undo. Stop/detach follows the selected session's ownership.

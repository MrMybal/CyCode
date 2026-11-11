---
id: project-tasks
title: Run multilanguage project tasks
description: Choose Cargo, npm or JVM tasks and understand their output and cancellation.
section: Work in CyCode
parent: projects
order: 122
revision: 1
---

## Select the task owner {#tasks}

Open **View > Project tasks** and select the actual project directory,
configuration, target and arguments. Detection recognises Cargo, npm, standalone
Java, Maven and Gradle descriptors. Build/Run commands use the selected adapter.
The GUI saves edited workspace documents before tasks; cancelling that save
prevents execution. Inspecting a project does not install dependencies or run it.

| Project | Choices and effects |
| --- | --- |
| Cargo | Package/workspace, bin/example/lib/test/bench, features, profiles, offline/locked options; builds run the project's build scripts and procedural macros |
| npm | Existing build/test/named scripts; dependency installation is a separate explicit action |
| Java | UTF-8 `javac`, annotation processing disabled, fresh class output; select a compiled public static main(String[]) entry |
| Maven / Gradle | Actual goals/tasks, optional module and resolved runtime classpaths; project build logic executes |
| Shader | Explicit DXC/glslang compilation; [shader settings](doc:shaders#compile), without GPU preview |

Cargo Run uses an actual compiled binary or executable example, not a guessed
filename or build-script executable. npm install uses `npm ci` with a lock file,
otherwise `npm install`, with the project's normal lifecycle scripts. Java debug
profiles use the selected real main class and runtime classpath; configuring a
profile is separate from starting the debug adapter.

## Read live output {#output}

In Build, select **Native build output** or **Project task output**. Each retains
its own latest bounded tail. Project output follows Cargo/npm/JVM/shader tasks;
long-lived Run terminals have their own lifecycle. Cargo protocol records display
readable compiler messages rather than metadata inventories. An oversized line
is explicitly omitted; old display bytes may be dropped without stopping the tool.

Click recognised source rows to open existing workspace files. Live task logs
use their validated working directory; unsupported paths stay text. A log is an
observation of the last run, not proof that source positions are current.
See [diagnostics](doc:diagnostics#output).

## Stop and run again {#cancel}

Cancel targets the current owned compiler/test/script process tree. Wait for the
running/cancelling state to finish; an acknowledgement alone is not termination.
Run dev servers belong in a persistent terminal and stop through that terminal.
Already-created outputs remain. Failed builds clear cached runnable metadata;
the next build still invokes the real tool. Project tasks are bounded by a
configured deadline and output limit, not by an unlimited background queue.

The shared [project operations](doc:api-project#operations) expose status/output
and execution separately. A changed output response replaces the old tail;
SDK clients must not append that whole tail as if it were a delta.

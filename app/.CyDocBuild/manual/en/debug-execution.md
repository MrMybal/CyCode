---
id: debug-execution
title: Choose how a paused program executes
description: Distinguish stepping, selected targets, run to cursor and moving the instruction pointer.
section: Work in CyCode
parent: debugging
order: 191
revision: 1
---

## Execution controls {#stepping}

Pause the program, select its thread/frame and open **Debugger > Execution
controls**. Adapter default omits stepping granularity; Statement, Source line
and Machine instruction require advertised support. Only selected thread also
requires a capability. Ordinary Step over/into/out shortcuts use these choices.
Continue uses the thread option but no stepping granularity.

Step back and Continue backward need actual adapter recording/history support.
CyCode does not emulate them with source Undo. Restart selected frame needs an
eligible inspected frame and adapter support; it can repeat I/O and other effects.
The last-request display distinguishes submission, acknowledgement and an
unconfirmed outcome. An accepted request is not a completed stop.

## Choose a call {#step-targets}

Use **Run > Choose step-in target**, then Read step-in targets. Select a function
actually returned by the adapter and explicitly Step into selected target.
Reading or selecting never resumes the target; no first choice executes
automatically. Refresh after changing frame, thread or pause. Target handles
expire on execution or refresh and must not be reused after an uncertain attempt.

## Run to cursor or set next statement {#destinations}

Run to cursor temporarily resumes toward an eligible source position. **Set next
statement** instead asks for valid destinations in the selected stack frame's
actual source. Enter a one-based destination line and optional UTF-16 column
(0 in the form means unspecified), read destinations, select one and explicitly
Set next statement here. It can skip code or repeat effects; it is not a normal
run-to operation or an Undo.

```diagram
{"type":"flow","title":"A guarded debugger action","nodes":[{"id":"pause","label":"Pause and select frame","x":0,"y":0},{"id":"read","label":"Read adapter choices","x":250,"y":0},{"id":"choose","label":"Review one target","x":500,"y":0},{"id":"execute","label":"Explicitly execute","x":750,"y":0}],"edges":[{"from":"pause","to":"read"},{"from":"read","to":"choose"},{"from":"choose","to":"execute"}]}
```

Frame/thread/session changes invalidate old choices. The adapter remains
responsible for executable/source correspondence; editing a source does not
recompile the paused program. See [inspection](doc:debug-inspection#inspect),
[breakpoints](doc:debug-breakpoints#data) and [debug operations](doc:api-debug#operations).

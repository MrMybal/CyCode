---
id: debug-inspection
title: Inspect and change a paused target
description: Use real adapter capabilities for memory, registers and breakpoints.
section: Work in CyCode
parent: debugging
order: 316
revision: 1
---

## Inspect a pause {#inspect}

Select the stopped thread and stack frame. Scopes / Registers exposes scopes reported by the adapter; explicitly load expensive scopes. Registers exist only when the adapter supplies such a scope. Expand variables in pages and re-evaluate watches after a new pause.

Memory needs a returned `memoryReference` and read-memory capability. Choose a bounded offset/count; unreadable bytes are reported separately. Disassembly can use the selected frame's instruction reference. Exception details require the corresponding adapter capability.

## Changes and breakpoints {#changes}

Variable/expression setters and memory writes are explicit process changes. Memory editing needs a current readable snapshot and both read/write support. A result can be uncertain if execution changes during a request; inspect again instead of blindly repeating a write.

Function, exception, data and instruction breakpoint controls depend on advertised capabilities. Inspect the adapter's verification/error result after submitting the requested set. Data and instruction handles belong to the observed session/pause; refreshing or resuming can invalidate them. Some operations replace the complete breakpoint set, so preserve the entries you want to keep.

Reverse stepping and frame restart are offered only by compatible adapters. Resume clears paused data. A late response from an earlier stop is rejected; select the new thread/frame and read again. Debugger changes cannot be rolled back through document Undo.

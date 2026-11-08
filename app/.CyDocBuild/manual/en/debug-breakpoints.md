---
id: debug-breakpoints
title: Data and instruction breakpoints
description: Review complete replacement lists and adapter verification before resuming.
section: Work in CyCode
parent: debugging
order: 192
revision: 1
---

## Watch a data access {#data}

In **Debugger > Data breakpoints**, pause and inspect a variable/expression in
the selected frame, or a child name in a displayed scope. Read the actual
description and supported access modes. Adapter default omits the mode instead
of guessing Write. Conditions and hit conditions need their own capabilities.

Add every desired inspected target to **Replacement list**, then Replace data
breakpoints. This replaces the complete set, not just the new row. New replacement
list discards the local draft and observes the current configuration; Clear
submits an empty list. Inspect verified/error messages, including hardware limits.
Session/pause-bound handles can expire; inspect again rather than inventing IDs.

## Stop at an instruction {#instructions}

Read Disassembly while paused. Right-click an eligible address to Add instruction
breakpoint or Copy instruction address. The instruction tab also accepts the
adapter's actual reference and byte offset. Use selected frame instruction copies
the inspected frame's reference.

New instruction replacement list starts from the known full configuration.
Review references, offsets, enabled flags, conditions and hit conditions, then
Apply instruction breakpoints. Typing/closing does not submit. Clear explicitly
submits an empty set. Resume, invalidation, changed configuration or successful
application discards the draft. These controls affect the debuggee and do not
participate in document Undo. Read [inspection](doc:debug-inspection#changes)
and [the public debug contracts](doc:api-debug#operations) before automation.

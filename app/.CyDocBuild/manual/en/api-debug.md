---
id: api-debug
title: Debugging
description: Public operations for Debugging.
section: API and extensions
parent: api-links
order: 112
revision: 2
---

Public operations for Debugging.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`debug_adapters`](doc:operation-debug-adapters-debug-adapters) — List native/plugin adapter identities without starting/probing them.
- [`debug_profile`](doc:operation-debug-profile-debug-profile) — Read the persistent debug/run profile.
- [`debug_configure`](doc:operation-debug-configure-debug-configure) — Update and atomically save supported debug profile fields in .cycode/launch.json, including the typed javaAttach endpoint.
- [`debug_status`](doc:operation-debug-status-debug-status) — Read actual DAP state, verified breakpoints, threads, stack, scopes, variables, watches and bounded adapter output.
- [`debug_breakpoints`](doc:operation-debug-breakpoints-debug-breakpoints) — Replace source breakpoints for one workspace file and persist them.
- [`debug_launch`](doc:operation-debug-launch-debug-launch) — Launch through the shared DAP owner.
- [`debug_attach`](doc:operation-debug-attach-debug-attach) — Attach through the shared DAP owner.
- [`debug_run`](doc:operation-debug-run-debug-run) — Run a trusted native executable or runnable .NET assembly in a native terminal without debugging.
- [`debug_continue`](doc:operation-debug-continue-debug-continue) — Continue the paused debuggee in the selected thread.
- [`debug_pause`](doc:operation-debug-pause-debug-pause) — Pause the running debuggee and asynchronously inspect its actual threads/stack.
- [`debug_hot_reload`](doc:operation-debug-hot-reload-debug-hot-reload) — Manually compile the imported JDT workspace incrementally (java/buildWorkspace=false), then send java-debug redefineClasses on the same Java DAP session.
- [`debug_cancel_reload`](doc:operation-debug-cancel-reload-debug-cancel-reload) — Signal cancellation before Java redefineClasses is sent.
- [`debug_next`](doc:operation-debug-next-debug-next) — Step over in the observed selected thread.
- [`debug_step_in`](doc:operation-debug-step-in-debug-step-in) — Step into through the common DAP owner, with optional capability-gated granularity/singleThread and queued or explicit session/stop/thread context.
- [`debug_step_out`](doc:operation-debug-step-out-debug-step-out) — Step out through the common DAP owner, with optional capability-gated granularity/singleThread and queued or explicit session/stop/thread context.
- [`debug_step_back`](doc:operation-debug-step-back-debug-step-back) — Execute one backward step through a debugger advertising supportsStepBack.
- [`debug_reverse_continue`](doc:operation-debug-reverse-continue-debug-reverse-continue) — Continue backward through supportsStepBack on the observed paused selected thread.
- [`debug_restart_frame`](doc:operation-debug-restart-frame-debug-restart-frame) — Restart the observed selected frame through supportsRestartFrame.
- [`debug_step_in_targets`](doc:operation-debug-step-in-targets-debug-step-in-targets) — Read actual step-in targets for the observed selected paused frame through supportsStepInTargetsRequest.
- [`debug_step_into_target`](doc:operation-debug-step-into-target-debug-step-into-target) — Enter an explicitly chosen host-issued target from debug_step_in_targets through the shared DAP stepIn owner.
- [`debug_stop`](doc:operation-debug-stop-debug-stop) — Stop the debug session.
- [`debug_run_to_cursor`](doc:operation-debug-run-to-cursor-debug-run-to-cursor) — Continue normally to a verified destination line using a session-only source breakpoint lease.
- [`debug_restore_run_to_cursor`](doc:operation-debug-restore-run-to-cursor-debug-restore-run-to-cursor) — Explicitly restore the run-to-cursor lease original source breakpoint list after an uncertain failure.
- [`debug_goto_targets`](doc:operation-debug-goto-targets-debug-goto-targets) — Read actual valid next-statement destinations in the selected frame source through supportsGotoTargetsRequest.
- [`debug_goto_target`](doc:operation-debug-goto-target-debug-goto-target) — Set the next statement through one DAP goto, using an issued debug_goto_targets handle and the same explicit observed sessionGeneration/stopGeneration/threadId/frameId.
- [`debug_evaluate`](doc:operation-debug-evaluate-debug-evaluate) — Evaluate an expression or debugger console command in the selected stopped frame.
- [`debug_variables`](doc:operation-debug-variables-debug-variables) — Read a bounded page of children or register-scope entries.
- [`debug_memory`](doc:operation-debug-memory-debug-memory) — Read up to 4096 bytes through the selected DAP adapter when it advertises memory reads.
- [`debug_set_variable`](doc:operation-debug-set-variable-debug-set-variable) — Assign a named child of a variablesReference through supportsSetVariable.
- [`debug_set_expression`](doc:operation-debug-set-expression-debug-set-expression) — Assign an expression through supportsSetExpression in the observed selected frameId.
- [`debug_write_memory`](doc:operation-debug-write-memory-debug-write-memory) — Write 1 to 4096 explicit bytes at an opaque memoryReference and optional offset.
- [`debug_disassemble`](doc:operation-debug-disassemble-debug-disassemble) — Read up to 200 actual instructions through a capable stopped DAP adapter.
- [`debug_exception_info`](doc:operation-debug-exception-info-debug-exception-info) — Inspect the actual exception reported for the selected stopped thread, if supported by this adapter.
- [`debug_data_breakpoint_info`](doc:operation-debug-data-breakpoint-info-debug-data-breakpoint-info) — Inspect a potential data breakpoint in the selected paused stack frame, or a named child of variablesReference.
- [`debug_data_breakpoints`](doc:operation-debug-data-breakpoints-debug-data-breakpoints) — Replace the entire session-owned data breakpoint list (empty clears).
- [`debug_instruction_breakpoints`](doc:operation-debug-instruction-breakpoints-debug-instruction-breakpoints) — Replace the complete session-only instruction breakpoint list (empty clears).
- [`debug_function_breakpoints`](doc:operation-debug-function-breakpoints-debug-function-breakpoints) — Replace and persist up to 128 function breakpoints.
- [`debug_exception_breakpoints`](doc:operation-debug-exception-breakpoints-debug-exception-breakpoints) — Persist exception filter IDs advertised by the actual adapter.
- [`debug_frame`](doc:operation-debug-frame-debug-frame) — Select a current stack frame and refresh its scopes, locals and watches.
- [`debug_thread`](doc:operation-debug-thread-debug-thread) — Select a current thread and inspect its stack.
- [`debug_source`](doc:operation-debug-source-debug-source) — Read an adapter-issued source reference while paused, including generated disassembly.

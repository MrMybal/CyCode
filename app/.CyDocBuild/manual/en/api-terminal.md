---
id: api-terminal
title: Terminals
description: Public operations for Terminals.
section: API and extensions
parent: api-links
order: 113
revision: 2
---

Public operations for Terminals.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`terminal_create`](doc:operation-terminal-create-terminal-create) — Start a persistent interactive shell in this IDE workspace.
- [`terminal_list`](doc:operation-terminal-list-terminal-list) — List this IDE instance’s persistent terminal sessions, including user-created sessions.
- [`terminal_read`](doc:operation-terminal-read-terminal-read) — Read a bounded terminal screen/scrollback range as plain Unicode lines.
- [`terminal_write`](doc:operation-terminal-write-terminal-write) — Write raw UTF-8/control input to an existing interactive terminal.
- [`terminal_key`](doc:operation-terminal-key-terminal-key) — Send an interactive terminal key.
- [`terminal_resize`](doc:operation-terminal-resize-terminal-resize) — Resize a persistent PTY and its emulated screen.
- [`terminal_clear`](doc:operation-terminal-clear-terminal-clear) — Clear retained scrollback without restarting the shell or changing files.
- [`terminal_close`](doc:operation-terminal-close-terminal-close) — Terminate and remove one terminal and its process tree.

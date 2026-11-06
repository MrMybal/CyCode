---
id: api-tests
title: Project tests
description: Public operations for Project tests.
section: API and extensions
parent: api-links
order: 106
revision: 2
---

Public operations for Project tests.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`tests_discover`](doc:operation-tests-discover-tests-discover) — Discover configured CTest tests through the actual CTest JSON model.
- [`tests_run`](doc:operation-tests-run-tests-run) — Run 1..128 selected issued CTest indices serially with per-test output/results and native process-tree cancellation.
- [`tests_cancel`](doc:operation-tests-cancel-tests-cancel) — Request termination of the currently owned CTest process tree.

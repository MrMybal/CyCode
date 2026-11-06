---
id: api-packages
title: Extension packages
description: Public operations for Extension packages.
section: API and extensions
parent: api-links
order: 104
revision: 2
---

Public operations for Extension packages.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`packages_list`](doc:operation-packages-list-packages-list) — Inspect installation-owned managed plugin selections, publisher trust and unused versions.
- [`packages_inspect`](doc:operation-packages-inspect-packages-inspect) — Verify a signed local plugin package directory, payload hashes, platform and permissions without installing/loading code.
- [`packages_install`](doc:operation-packages-install-packages-install) — Install or update a reviewed signed package from a workspace directory.
- [`packages_remove`](doc:operation-packages-remove-packages-remove) — Remove a managed plugin from the next-start selection after dependency validation.
- [`packages_rollback`](doc:operation-packages-rollback-packages-rollback) — Select one retained signed version after trust, payload, platform and dependency checks.
- [`packages_clean`](doc:operation-packages-clean-packages-clean) — Delete one verified unused managed package using an exclusive lease and exact known file paths, without recursive deletion.

---
id: api-documents
title: Documents
description: Public operations for Documents.
section: API and extensions
parent: api-links
order: 111
revision: 2
---

Public operations for Documents.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`documents_reload`](doc:operation-documents-reload-documents-reload) — Reload a document from disk with a fresh revision.
- [`documents_list`](doc:operation-documents-list-documents-list) — List documents currently open in the IDE.
- [`documents_read_page`](doc:operation-documents-read-page-documents-read-page) — Open native paged UTF-8 text and read at most 256 KiB at a time.
- [`documents_prepare_byte_edit`](doc:operation-documents-prepare-byte-edit-documents-prepare-byte-edit) — Prepare one edit of an already-open paged document using UTF-8 byte boundaries and expectedRevision.
- [`documents_save_paged`](doc:operation-documents-save-paged-documents-save-paged) — Explicitly stream an already-open paged document to its original workspace file using the shared atomic writer.
- [`documents_close_paged`](doc:operation-documents-close-paged-documents-close-paged) — Close a native paged live document with its expected revision.
- [`documents_read`](doc:operation-documents-read-documents-read) — Read the live document and its revision, including unsaved edits.
- [`documents_create`](doc:operation-documents-create-documents-create) — Create an empty UTF-8 file inside the workspace.
- [`documents_create_untitled`](doc:operation-documents-create-untitled-documents-create-untitled) — Create an in-memory draft with an issued untitled reference, optional filename for lexical coloring and initial text.
- [`documents_save_as`](doc:operation-documents-save-as-documents-save-as) — Save the current live text into a NEW workspace file in an existing folder and rebind the same document identity.
- [`documents_replace`](doc:operation-documents-replace-documents-replace) — Replace the live buffer using expectedRevision.
- [`documents_save`](doc:operation-documents-save-documents-save) — Atomically save a live buffer, preserving BOM and line endings.
- [`documents_close`](doc:operation-documents-close-documents-close) — Close a live file or issued untitled draft with an expected revision.

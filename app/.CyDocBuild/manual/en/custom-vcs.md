---
id: custom-vcs
title: Develop a revision-control provider
description: Expose your backend’s real actions through ABI2 without imitating Git.
section: Work in CyCode
parent: plugin-api
order: 281
revision: 1
---

## Register the provider {#provider}

Use the [native plugin contract](doc:plugin-api#contract), an independent plugin
ID and a `vcs` contribution under its namespace. Declare only implemented
capabilities. A backend can publish changes or manage leases without a Git
staging area, remotes or Push/Pull semantics.

```json
{
  "id": "example.revisions.provider",
  "title": "Example revisions",
  "capabilities": ["status", "diff", "publish"],
  "actions": [
    {"id": "diff", "title": "Working differences", "scope": "repository", "fields": []},
    {"id": "publish", "title": "Publish changes", "scope": "repository",
     "fields": [{"id": "message", "title": "Description", "required": true}]}
  ]
}
```

This is a descriptor example, not an implemented backend. Supply its command
callback and compatible manifest. Status returns workspace-relative paths and
actual backend metadata. Diff supplies a unified patch for Review; log/blame
provide read-only review text. Unsupported binary/conflict views need an explicit
panel or contract instead of fabricated Git text.

## Routing and access {#routing}

Project mappings in `.cycode/vcs.json` select the deepest matching root; Git
remains the built-in default. An unavailable mapped provider stays unavailable
until explicitly reenabled/reinstalled or remapped. Provider forms/menus use
its actions. Typed Git operations remain Git-specific; the generic
[vcs_provider_action](doc:operation-vcs-provider-action-vcs-provider-action)
uses the provider route and conservative plugin/process/workspace/VCS permissions.

Recognised read actions are status/inspect/diff/log/history/show/blame. Other
IDs, even a custom read-like name, are treated as mutations and receive dirty-buffer
and disk guards. Copy host results before the next request; catch exceptions
across the C boundary and join owned work before deactivation/shutdown returns.
Native DLLs run as trusted application code, without process isolation.
Authentication, remote publication and history semantics remain the backend's
responsibility. See [project mappings](doc:version-control#providers).

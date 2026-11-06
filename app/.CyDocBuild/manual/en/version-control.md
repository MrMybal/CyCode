---
id: version-control
title: Choose version control
description: Use Git by default or map a project folder to another provider.
section: Work in CyCode
order: 220
revision: 2
---

## Project mappings {#providers}

Git is built in. Plugins can provide Perforce, SVN, Mercurial, Diversion or another compatible revision-control system. Map a project folder to its provider; the most specific folder mapping determines the applicable menus/actions.

The installed provider must advertise the requested capability. Provider menus and forms differ; an external client or server is not installed merely by selecting its provider. A workspace can contain different mapped roots.

For built-in Git, use the [Git workflow](doc:git#changes). For external tools, see [provider setup](doc:providers#external). The SDK's typed `vcs_*` Git operations remain Git-specific; external providers use `vcs_provider_action`.

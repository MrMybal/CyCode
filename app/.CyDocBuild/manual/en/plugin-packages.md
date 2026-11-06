---
id: plugin-packages
title: Install and update an extension
description: Inspect a signed package and manage publisher trust.
section: Work in CyCode
parent: plugins
order: 311
revision: 1
---

## Inspect and install {#install}

1. Open Extensions > Plugin packages and choose a local package folder.
2. Select Inspect package. Read the version, platform, compatibility, permissions and publisher fingerprint. Inspection does not load the native library.
3. Verify the fingerprint independently. If necessary, explicitly trust that publisher, then inspect again.
4. Choose Install / update package as a separate action.
5. Start a new application process to use the new selection. Existing hosts retain their pinned library version.

This manager accepts signed local packages from trusted publishers; it does not download a marketplace catalogue. A signature identifies publisher/content, not an OS sandbox. Per-project activation in Extensions remains separate from installation-wide package selection.

## Rollback and remove {#versions}

Updates must increase the numeric version and keep the publisher key. To change publishers, remove the package and inspect the replacement afresh. The selected version and four previous versions are retained. Select previous version verifies compatibility and payload before switching.

Removing a package checks required dependents. Cleanup touches only verified, unused versions outside selection/history; a version pinned by another host stays in use. Revoking a publisher affects future discovery and cannot unload code already running. Bundled extension IDs cannot be replaced by managed packages.

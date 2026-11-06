---
id: plugins
title: Manage extensions
description: Enable trusted plugins, inspect options and manage packages.
section: Work in CyCode
order: 250
revision: 2
---

## Enable an extension {#extensions}

Open the extension manager to inspect installed plugins, commands, dependencies and options. Enable or disable a plugin for the workspace, then inspect the contributed menus and panels. Disabled native libraries can remain loaded until application shutdown.

Native extensions execute trusted code in the application; they are not a sandbox. Project/provider commands can also run external tools according to their declared permissions. Review the plugin's origin and capabilities before enabling it.

The package manager provides explicit inspect/install/remove/rollback/clean actions with reviewed paths and signature/trust decisions. It is not a general public extension marketplace, and it does not make arbitrary native code safe.

## Write an extension {#development}

Use the public C ABI and JSON contribution descriptors for commands, menus, panels, settings, syntax, project/VCS providers and supported language/debug services. See [plugin development](doc:plugin-api#contract) for ownership and entry-point rules.

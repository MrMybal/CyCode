---
id: providers
title: External revision-control providers
description: Configure Perforce, SVN, Mercurial or Diversion through plugins.
section: Work in CyCode
order: 240
parent: version-control
revision: 2
---

## Configure a provider {#external}

Install/enable the trusted provider plugin, install its normal client and configure that client's workspace/authentication outside the IDE where required. Map the project folder to the provider, refresh its context and use its declared menu actions.

Perforce uses the P4 client and its workspace/server configuration. SVN, Mercurial and Diversion use their respective installed clients. Available changelist, shelf, stream and conflict workflows depend on the plugin's advertised actions; selecting a provider does not promise complete parity with its standalone client.

Providers can contribute different menus and forms. A custom system can use the same versioned [plugin contract](doc:plugin-api#contract) without replacing the built-in Git service.

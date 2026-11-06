---
id: perforce
title: Use Perforce (P4)
description: Map an existing P4 client and review supported file actions.
section: Work in CyCode
parent: providers
order: 312
revision: 1
---

## Configure a client {#setup}

Install the P4 CLI and configure its server, client, view, ticket and trust using the normal P4 tools. An absolute `CYCODE_P4` can select the executable. Enable `cycode.perforce` in Extensions. In Version control, select Perforce (P4) and map the project directory inside the configured client root. Opening or mapping a project does not create a client, log in or sync.

## Daily work {#files}

Status shows opened project files, including action, changelist, type and unresolved state. It is not an automatic scan of unopened offline changes. Select a file for Diff, History or Blame. Checkout uses `edit`; Add and Reopen accept a default or positive numbered changelist.

Commit submits one resolved edit/add/delete file in the default changelist with an explicit description. Use an external P4 client for complete numbered changelists, shelves, streams, moves and integrations. Revert requires `REVERT` and can discard disk changes; document Undo does not restore them.

Sync is project-scoped and refuses opened files or changes detected by its read-only reconcile preflight. Configure P4IGNORE for appropriate generated files and `.cycode` metadata. Lock/unlock acts on one selected file without administrator force. Unsupported or ambiguous client mappings are reported explicitly.

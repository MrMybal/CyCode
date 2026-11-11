---
id: limits
title: Capabilities and limits
description: Understand tool dependencies and the boundaries of the IDE.
section: Start here
order: 300
parent: overview
revision: 3
---

## Scope {#limits}

CyCode provides native editing, project tools, language-service connections, Git/provider workflows, debugging and extensions. It does not promise complete Rider, Visual Studio or VS Code feature parity. Language servers and adapters decide which semantic or debugging actions are available.

The ordinary editor/LSP buffer profile handles text up to 4 MiB. The native paged UTF-8 profile and shared CyUI virtual view are separate components; their presence does not establish complete graphical integration or a live LSP overlay for paged edits. Saving a paged document publishes its complete native content, never just a visible fragment. Shader compilation has no GPU preview. Web tasks have no built-in browser debugger. Blueprint asset editing, complete Unreal Editor integration and unrestricted native-plugin isolation are not supplied. The structured test explorer uses CTest rather than every framework.

Service readiness is distinct from backend initialization, current-file diagnostics and whole-project semantic coverage. Restored traversal checkpoints contain historical observations, which remain unknown for the current pass until reanalysed. The same native bridge supplies embedded service status and its endpoint; that identity is not evidence of complete indexing. See [analysis progress](doc:analysis-progress#persistence) and [native integration](doc:native-core#ownership).

## Languages {#translations}

This manual is available in English and French. Technical API identifiers, source-language keywords and literal filenames retain their original spelling. Switch languages in the header to keep the current page.

## Reader {#reader}

The complete manual opens locally without a server or network connection. Ctrl+K searches it. Media enlarges inside the reader and Escape reduces it. Browser storage governs bookmarks, notes and remembered positions. External licence/repository links need a connection.

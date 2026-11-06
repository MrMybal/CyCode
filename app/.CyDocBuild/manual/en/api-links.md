---
id: api-links
title: Public SDK and MCP reference
description: Find operation signatures, schemas and shared editing rules.
section: Work in CyCode
order: 270
parent: plugins
revision: 2
---

## Operation contract {#contract}

The following service subpages list the actual operations declared by CyCode. Each operation documents its identifier, parameters and input/output schemas. These identifiers are not direct C++ function names. Invoke them through CyToolsCore or the MCP adapter; native plugins use their advertised host methods.

Optional parameters are marked `?`. `expectedRevision` must come from a fresh document read; issued preview/session/plan IDs must come from the corresponding native request. Do not manufacture these values or reuse them after changing their source context.

For example, select the MCP tool `op_documents_read` with these arguments for a file that exists in your workspace:

```json
{"parameters":{"path":"Source/main.cpp"}}
```

Read its actual returned revision before a later `op_documents_replace` call. The [document operation](doc:operation-documents-read-documents-read#signature) shows the complete result schema. Parameter tables and schemas describe the operation's input; official MCP tools wrap that input in `parameters`, as above. This is the tool's argument object, not a complete transport envelope.

## Review, apply and save {#transactions}

1. Inspect the current document, repository or process state.
2. Request a preview/plan when the operation provides one.
3. Apply with the returned identity and guards, after any required confirmations.
4. Inspect the actual result. Save live document changes explicitly.

```diagram
{"type":"flow","title":"Document transaction","nodes":[{"id":"read","label":"Read + revision","x":0,"y":0},{"id":"preview","label":"Review preview","x":220,"y":0},{"id":"apply","label":"Apply in memory","x":440,"y":0},{"id":"save","label":"Save to disk","x":660,"y":0}],"edges":[{"from":"read","to":"preview"},{"from":"preview","to":"apply"},{"from":"apply","to":"save"}]}
```

`changes_undo` concerns supported native editor transactions; Git commits, process execution and debugger memory changes are different operations. A successful transport/job status does not guarantee a zero process exit code. See [MCP connection](doc:mcp#operations) and [plugin ABI](doc:plugin-api#contract).

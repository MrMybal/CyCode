# C# language services

CyCode routes C# documents to Roslyn through csharp-ls and C/C++ to clangd. Each
backend has its own owner thread, process and queue; both use the same native
Engine documents, revisions, edit transactions and bounded LSP transport. The GUI
starts a server when a supported document is opened. It passes the selected .sln
explicitly; direct .csproj workspaces use server discovery. Project references,
references restored by the .NET SDK and unsaved open documents feed semantic results.
For the pinned C# backend, didOpen starts lazy workspace loading after initialization;
the server serializes these notifications and retains their overlays. Semantic
queries wait for its loading progress to finish. The initialize response alone does
not mean Roslyn has loaded the workspace. Unsaved documents are replayed on restart.

The local package is checksum-pinned csharp-ls 0.28.0, targeting .NET 10, with a
private .NET SDK 10.0.403 under runtime/tools/csharp-dotnet. build.py or scripts/setup_csharp.py
installs both only under this project. The SDK archive has its official SHA-512
locked per platform; the server package has a locked SHA-256. No global dotnet tool
or SDK is installed or changed. Builds retain their existing MSBuild/dotnet selection.
The project's own target SDK/targeting packs and restored dependencies must be
available; the private analysis SDK does not replace every SDK requested by global.json.
Exact URLs/hashes and upstream source are in language-tools.lock.json.
CYCODE_CSHARP_LS can select an absolute server executable or DLL;
CYCODE_CSHARP_DOTNET can select its absolute .NET host. Missing components and
project-load failures appear in Problems.

The desktop uses the ordinary completion menu, definition/reference navigation,
type/symbol search and file structure, Shift+F6 rename preview/apply/undo and
document/selection formatting commands. Workspace symbol search follows the active
document's language. Rename edits are prepared by Roslyn, then validated and applied
to live buffers by Engine; Save all remains explicit. C# formatting uses Roslyn and
.editorconfig. Revision changes during a query reject stale results.

Starting a C# server evaluates trusted MSBuild projects/imports. A native CLI/MCP
Engine keeps C# disabled until language_configure is called with process.execute.
Reading documents or language_status does not enable it. The desktop's normal
language-service activation and Restart language server action enable both backends;
capture-only mode starts neither. language_configure accepts enabled and an optional
executable and affects only the current process. An enabled server starts lazily on
an open C# document/request. It can be disabled or restarted without losing buffers.

MCP operations language_complete, language_definition/references/document_symbols,
language_rename and language_format select a backend from the document extension.
language_symbols accepts language=cpp (default) or csharp. language_status includes
servers.cpp/servers.csharp and merged diagnostics. Completion returns standard LSP
items; coordinates are zero-based UTF-16, including supplementary characters.
The GUI filters Roslyn items against the typed prefix and tracks editor transactions
after popup insertion, so accepting a suggestion immediately updates the shared live
buffer and dirty revision. Snippet/import/command-bearing items are not inserted by
this plain-text completion UI.
Server requests to apply workspace edits are refused: use the revision-checked
preview/apply API. No C# server metadata URI grants arbitrary filesystem access.

Roslyn semantic tokens now augment the ordinary lexical colorizer through the same
revision-checked language_highlight service used for C++. Cross-project C# semantic
colors are exercised in a hidden native desktop; no separate editor model is used.
Edits/restarts discard old spans while lexical colors remain immediately available.

Each backend allows 32 outstanding requests. C# has a 60-second request deadline,
including project loading; C++ retains 15 seconds. Framed messages, stderr and queues
are bounded. Process-tree shutdown is tied to workspace/IDE shutdown, disable and
restart. Filesystem watchers/dynamic registration are not implemented; restore/build
the project, then restart the server after changing project references or SDKs.

Managed run/debug uses NetCoreDbg through the shared native DAP service, including
portable PDB source breakpoints, conditions, locals, objects/arrays, watches,
evaluation, stepping and attach/detach. Select the built executable assembly from
MSBuild evaluated artifacts and use the ordinary Debugger panel and Rider bindings.
See [Execution](Execution.md) for the tested .NET 8 scope and adapter limitations.

Remaining work includes code actions/import insertion, snippets,
completion resolve, hover/signature UI, metadata decompilation/source-generated views,
Razor, test discovery and broader framework/SDK/project-load coverage. External
project files outside the workspace remain outside editable document permissions.
Windows is validated locally; Linux/macOS execution remains unverified.

Reproduce with scripts/build.py, private Python's test_mcp.py suite, then
scripts/verify_csharp.py and scripts/verify_csharp.py --preview-only. Tests use
owned workspaces, two .NET 8 projects and the locally installed target SDK/packs;
they restore with empty NuGet package sources and leave source files unsaved.

Upstream: [csharp-ls 0.28.0](https://www.nuget.org/packages/csharp-ls/0.28.0),
[source and license](https://github.com/razzmatazz/csharp-language-server/tree/0.28.0),
[official .NET release metadata](https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json).

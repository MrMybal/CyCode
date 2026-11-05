# Incremental semantic token updates

The existing native language worker now negotiates LSP full/delta semantic tokens.
This applies to clangd, Roslyn and built-in or plugin language servers that
advertise full-document tokens with delta support. The desktop and the existing
language_highlight SDK/native-plugin service share this transport; no public
operation, permission, CyUI dependency or editor model is added.

The first request is full. When the server provides a resultId, subsequent
queries may send textDocument/semanticTokens/full/delta with that opaque ID.
Edits reconstruct the previous integer array before the existing UTF-16/Unicode,
legend, overlap and display-budget checks. Offsets refer to the same old array,
not token numbers; edits need not arrive sorted or on token boundaries.
Equal-position insertions preserve their response order. Full replacements,
empty edit lists and null results are supported. Omitted resultId forces the
next request to be full; the empty string remains a valid opaque ID.

Reference: the official [LSP 3.17 semantic token specification](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/semanticTokens.md).

## Cache ownership and invalidation

Each independent native language worker owns an in-memory LRU cache. Its limits
are 16 documents and 4,000,000 uint32 values (16,000,000 bytes of integer data,
plus IDs/container overhead). Each result holds at most 1,000,000 values and its
opaque ID at most 4 KiB. A delta contains at most 4,096 edits and 1,000,000 inserted
values; the final array also respects the result limit. Negative, fractional,
oversized, out-of-bounds, overlapping or incomplete results are refused.

Requests retain immutable snapshots, so eviction or a concurrent response cannot
change a delta's base. In-flight snapshots remain alive until the existing
bounded request finishes; the cache budget does not include these retained
snapshots or transient JSON/decoding buffers. No snapshot or result ID is persisted
or exposed as a document reference. Decoded SDK results keep their existing shape.

Ordinary edits retain the previous server base, as required for delta updates.
Opening/closing a document, stopping/restarting a backend, and a server
workspace/semanticTokens/refresh request discard affected bases. A separate
refresh generation distinguishes those resets from ordinary document edits.
Responses whose document/semantic generation changed are rejected. Engine
revision guards and the editor's immediate return to lexical colors remain active.

## Failure and fallback

An RPC failure or structurally malformed delta discards its base and retries one full request
within the original query deadline. The worker retains the same request/promise
and outstanding-query accounting; a retry does not extend the timeout or start
another process. If that full request fails, the existing caller receives an
error and lexical coloring remains available. Stale generations are rejected
without retrying in the obsolete context.

Raw array validation occurs before caching; source positions, token types,
modifiers and Unicode boundaries retain the existing decoder's checks. An invalid
source-position/full response produces an error rather than display data. This
change reduces the protocol payload when a backend supplies a smaller delta;
it still reconstructs and decodes a full document for the existing overlay.
No latency, memory or throughput improvement has been measured locally.

## Scope and verification

Range/viewport requests, advanced modifier styling, semantic-request cancellation,
dynamic capability registration and broader backend benchmarks remain separate
work. Servers without delta support continue to use full requests. Reader's
shared lexical coloring implementation is unchanged.

semantic_token_tests.cpp extends the existing opt-in cycode_tests target; no new
runner is introduced. Assertions cover unsorted and integer-granularity edits,
empty/deletion/full/null responses, immutable bases, Unicode decoding, integer/
edit/result bounds and LRU eviction. These are currently checked with MSVC /Zs
only, with no object, test executable or test execution. Actual server delta
negotiation, fallback/deadline/refresh concurrency, GUI behavior, themes/DPI and
Linux/macOS remain unexecuted during the Kaspersky interruption. See
[Validation](Validation.md) for the production build and source/schema checks.

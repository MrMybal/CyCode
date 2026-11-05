# Workspace events and native subscriptions

The native Engine owns one bounded event journal per workspace instance. GUI,
MCP and ABI2 plugins observe that same stream. It reports committed document,
resource and build metadata; it is not an alternative document model or an Undo
log. Events contain no buffer text. Services such as `documents.read` remain the
authority for a fresh snapshot and for revision-checked changes.

## Reading the journal

`events.read` (MCP `events_read`) is read-only and does not activate language
servers, refresh plugins or acquire the document/build execution locks. Example:

```json
{"tail":true}
```

This returns an empty page and the current `epoch` / `nextSequence`. Take that
cursor **before** reading a snapshot, then resume with:

```json
{"epoch":"<issued epoch>","afterSequence":42,"limit":128,"types":["document.*","build.finished"]}
```

The response contains `epoch`, `oldestSequence`, `latestSequence`, `nextSequence`,
`gap`, `hasMore` and `events`. Each event has `sequence`, `type`, `origin`,
`timestampMs` and `data`. Sequences are monotonic within the issued epoch; they
are not document revisions. An empty origin means a host operation. Plugin host
requests record the owning plugin ID. `excludeOrigin` can omit one such owner.

Always advance to `nextSequence`, including an empty filtered page. Read additional
pages while `hasMore` is true. Filtering accepts up to 16 exact types or trailing
wildcards (each at most 64 bytes). The page limit is 1–512, default 128. A nonzero
resume cursor requires its issued epoch; foreign epochs and future cursors are
errors. Omit the cursor to inspect retained history, or use `tail` to start now.

Retention is at most 512 entries / 4 MiB. Entries larger than 64 KiB are dropped
with a gap marker; allocation/serialization failure also marks a gap. A consumer
that falls behind must take a new snapshot when `gap` is true. Epochs change on
workspace replacement/restart and on sequence rollover. Nothing is persisted, and
there is no guarantee of replay after a consumer crashes.

Current event families:

| Type | Metadata and meaning |
| --- | --- |
| workspace.opened | Root and selected entry of this native instance |
| workspace.changed | Filesystem reconciliation summary; rescan affected resources |
| document.opened/changed/saved/reloaded/closed | Relative path, stable document ID and revision; dirty/conflict state where applicable |
| transaction.committed | Applied/undone shared semantic change set; refresh its affected documents |
| resources.changed | Committed resource operation; refresh resource/document snapshots |
| build.started/finished | Native operation ID, method and completion/cancellation/failure metadata |

Multi-document change events are published after the complete in-memory transaction
has been applied. Resource/reload operations retain existing document identities.
The journal provides ordering and change notification, not a multi-service snapshot
transaction. A later mutation may already have occurred when a consumer reads.

## ABI2 automatic subscriptions

Check the optional host tail size and
`CYCODE_HOST_FEATURE_EVENT_SUBSCRIPTIONS_V2` before registering an `events`
contribution during init:

```cpp
host->contribute(host->context, "events",
    R"({"id":"example.tools.observe","types":["document.*","build.finished"],"maxBatch":32})",
    on_events);
```

The callback uses the normal `CyCodeCommandV2` signature and returns plugin-owned
JSON, for example `{"ok":true}`. IDs must belong to the plugin namespace. No
`when`/GUI context is supported; `maxBatch` is 1–32 and the host accepts at most 32
subscriptions. A separate `CYCODE_HOST_FEATURE_EVENTS_V2` bit advertises the
read-only `events.read` host request without requiring automatic callbacks.

The native worker calls active subscribers serially with all other callbacks for
that plugin host, outside Engine document locks. It attempts delivery every 100 ms,
skips a busy host and fairly limits each pass to eight callback deliveries. This
is not a real-time notification deadline. Callback envelopes have the journal
page fields plus `subscription` and `event`:

- `subscribed`: initial empty page, cursor at the current tail, `gap:true`.
  Read the needed state through host services. Mutating host requests are denied
  during this initial snapshot handshake.
- `events`: a normal filtered batch. Host requests use the plugin manifest's
  permissions and the existing native revision/path/process guards.
- `gap`: empty page advanced to the latest tail. Rebuild the snapshot with
  read-only host requests; mutations are denied during this callback too.

The callback's successful return acknowledges that page. A thrown/returned error
pauses only this subscription and is displayed in Extensions and
`plugins.list.eventSubscriptions`. Disable/re-enable the plugin to retry from a
new snapshot; failed callbacks are not retried automatically. Activation changes
reset subscription cursors. Disabling removes future delivery; libraries remain
pinned until shutdown.

Events produced synchronously by the subscribing plugin's own host requests are
excluded from its automatic stream. This prevents the ordinary self-edit loop;
it does not prevent two plugins reacting to each other, or identify filesystem
writes performed directly by native code. Plugins must coalesce work, check
revisions and bound their callbacks. Follow-up work on plugin-owned threads cannot
call host services; use a later host callback.

Engine shutdown rejects new work, stops delivery, cancels owned work until the
current callback returns, then deactivates/releases plugins while host services
are still alive. Lifecycle/shutdown hooks themselves cannot call the host.
Native code is trusted: an unreturning callback, native crash or arbitrary OS
access is not isolated or preempted. There is no separate plugin process sandbox.

The bundled Unreal extension observes workspace/resource/build changes and shows
the last event and count in its existing overview. These events do not implement
Blueprint asset analysis, Live Coding or UnrealEditor automation.

## Validation

Windows x64/MSVC production application/core/CLI and bundled plugins compile with
tests disabled. Journal paging/overflow/origin/ordering assertions, automatic
subscription/permission/error/activation fixtures and an official SDK/MCP test
are present; they have **not been executed** after the Kaspersky interruption.
MSVC syntax-only checks generate no test objects or executables. GUI acceptance,
shutdown under runtime load and Linux/macOS execution remain pending. See
[Validation](Validation.md) and [Testing](Testing.md).

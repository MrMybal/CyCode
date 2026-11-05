# Workspace sessions

The new Structure panel uses the existing panel visibility and docking metadata;
its default is hidden. Its filter, ordering, follow-caret state and the palette's
explicit symbol-server selection are current workspace view state and reset on
workspace switch/restart. No symbol/node/result cache is persisted. See
[Symbol navigation](SymbolNavigation.md).

The desktop remembers workspace-relative open files, the active tab, native editor
cursor coordinates, document scroll anchors, word wrap, visible code folds,
image viewing controls, recent files, panel visibility
and the common ImGui docking/window layout. Options > Restore last
workspace session controls reopening. Explicit command-line file/line selection
takes precedence. Documents restore at most two per frame through the existing
workspace/open guards; removed or inaccessible files are skipped.

The session also retains explicit Git repository selection and its observed marker
identity. Automatic selection is stored separately from an explicit workspace root.
An unavailable/replaced repository produces a warning and restores Automatic;
other tabs/layout still restore. The optional versioned `git` record is bounded
and rejects absolute/traversal/metadata/untitled paths or mismatched empty identity.
Legacy records without it remain valid. This desktop preference does not change
project VCS mappings or explicit SDK/plugin repository parameters. Filesystem/Git
identity restoration and multi-instance GUI acceptance remain unexecuted under
the native-test suspension; see [Git worktrees](GitWorktrees.md).

Each workspace entry has a separate session directory beneath the application's
`data/settings/workspaces`. Every activation owns a unique JSON record, INI file
and nonblocking OS file lease. Two running instances never select each other's
live session as a restore source or write the same layout file. The next instance
uses the newest valid inactive record, including an abandoned crash record. A
deliberately empty closed session supersedes older open-tab lists. Legacy shared
INI files remain a one-time layout fallback.

Metadata checkpoints run on a background worker every five seconds after pending
restoration finishes. Workspace switches and normal exit wait for the outstanding
write and save the final metadata. Atomic writes compare the previous bytes and
refuse external modifications. Record identity, relative paths, nesting and
collection sizes are checked. Limits are 256 tabs, 100 recent files, 512 panels,
512 KiB layout and 2 MiB record; the newest 32 valid inactive records are retained.
Live records are never pruned. Lock files retain their inode to preserve leases.

The app owns this metadata; CyUI still owns editor rendering and layout. No text,
credentials, process state or native plugin objects are serialized. Registered
SDK/debug source references and external image grants do not restore. Terminal
processes, debugger sessions, complete editor Undo history, multiple cursor/
selection sets and horizontal text scroll do not restore. Legacy scroll metadata
remains approximate; new records retain the top visible document line, so wrapping/
folding changes do not confuse a visual row with a source line.
Untitled tab coordinates/window keys now restore when their draft is explicitly
recovered through Recovery; text is not automatically reopened. See
[Untitled documents](UntitledDocuments.md). Dirty text remains in the
existing [Recovery](Recovery.md) checkpoints and is only applied through that
guarded recovery workflow; reopening a tab does not silently overwrite disk.

`WorkspaceSession` and `NativeFileLease` are UI-free native services. The desktop
adapter captures and restores common editor/layout models without a second editor
implementation. Debug launch-profile writes also use the same cross-instance
lease around their disk-baseline check and atomic write.

## Editor views

Optional version-1 editor/image fields extend the existing version-1 record;
older records remain readable. The shared CodeEditor still owns wrap, folding,
rendering, cursor normalization and Undo. No vendor code or second folding model
is introduced. Pending settings survive a checkpoint while a tab is hidden.

Wrap applies when the tab opens. Fold starts apply through the public backend
API after its first visible render has discovered folding opportunities. The
stored fingerprint is compared with the current live text before folding;
changed source keeps wrap/cursor restoration without replaying stale fold starts.
This uses Recovery's existing FNV-1a/length fingerprint for accidental-change
detection, not authentication. The cursor retains the backend's normal behavior:
making it visible may unfold a containing block. Explicit editor commands,
navigation or input take precedence over a pending fold/cursor restoration.
Explicitly recovered untitled documents use the same adapter and fingerprint
guard; Recovery still owns the text transfer and Undo transaction.

Only fold starts represented by the backend's public IsLineFolded query are
captured. Hidden nested-fold bookkeeping is not exposed; restoring that internal
history is outside this integration. Limits are 4,096 starts per tab and 16,384
per session. Capture scans at most 200,000 source lines per document and 1,000,000
across a checkpoint, with 32 MiB of newly fingerprinted text. Fingerprints are
cached by document identity and the native editor's text revision. If a capture exceeds these bounds, its
folds are omitted and foldsComplete is false; wrap, cursor and anchor are retained.
Non-workspace sources do not consume these capture budgets.

## Image views

The application-owned ImageView adapts CyUI's existing Canvas/ImageState. It
stores pixelated filtering, Fit mode, or manual zoom plus the image-space point
under the viewport center. Restoration waits for decoding and a visible viewport;
manual framing uses that new viewport's dimensions, rather than old screen pixels
or a monitor position. Pending settings survive asynchronous loading/checkpoints.
If source dimensions changed, the image fits again while keeping the filter.
Fit mode also follows viewport resizing; manual views retain their zoom/pan.
Reload and explicit viewing controls supersede pending settings.

Camera metadata accepts finite zoom 0.001–1,000, center coordinates within
±10,000,000 image pixels, dimensions up to the existing 32,768-pixel bound, and
strict boolean flags. Transient pan/crop gestures, texture IDs and decoded pixels
are never serialized. External image grants and SDK/debug sources still do not
restore. No file content is written by these settings.

## Verification

Windows production compilation and MSVC syntax-only checks cover the implementation
and added contract sources. Native lease/concurrency tests and graphical
restoration acceptance remain unexecuted during the antivirus interruption.
Linux/macOS implementations have not been compiled or executed locally.

View-state assertions extend the existing core/workflow-UI/image test targets,
without an additional runner. Only their MSVC /Zs syntax is checked at present;
actual folding, Unicode anchors, asynchronous image/filter framing, user-input
precedence, themes/densities/DPI and multi-instance restoration remain unexecuted.
See [Validation](Validation.md) for the production build and source checks.

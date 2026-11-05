# Diversion provider

The bundled native ABI2 extension `cycode.diversion` contributes
`cycode.diversion.provider`. Enable it in Extensions, choose Diversion in Version
control and map the exact directory containing `.diversion/`. Git remains the
default for unmapped projects. Install Diversion separately, log in and clone/init
with its own client before using this provider. Tool discovery checks an absolute
`CYCODE_DV`, managed tool locations and PATH; CyCode does not redistribute the CLI
or collect credentials. No dependency or API subscription is added to the build.

## Operations

Diversion tracks working changes and synchronizes through its own agent. There is
no staging area or push/pull action. The provider follows the documented
[agent workflow](https://docs.diversion.dev/agent-quickstart) and uses `dv`
subcommands, never its interactive shell. Mapping/refresh does not invoke init,
clone, login, browser launch, preference changes or an explicit write command.
The installed client's normal network/agent behavior remains in effect.

The ten menu actions use the existing common provider forms and Review views:

| Action | Behavior |
| --- | --- |
| diff | Working-copy patch through shared CyUI Diff, with color disabled. Native requests can select one file. |
| log / history | Read-only CLI history, default 50 and maximum 200 records, ISO dates; optional exact local file operand. No structured commit fields are guessed from display text. |
| blame | Read-only annotations of one selected file. |
| commit | Commit all changes with an explicit nonempty message; a native request with `path` commits that file only. The client's automatic synchronization publishes the result. Hooks remain enabled. |
| revert | Reset one file after entering `REVERT`. Never reset all, clean added files, or select a forced conflict policy. Disk restoration belongs to Diversion and is not an editor Undo transaction. |
| branch.create | Create a named branch with `--no-checkout`, preserving the current branch. |
| branch.switch | Checkout a simple branch/ref only with a clean, fully synced working copy. Do not carry, shelve or discard local changes, or apply the target shelf. |
| update | Update a clean, fully synced workspace using manual conflict resolution and wait for local synchronization. |
| lock / unlock | Acquire/release a lock on one selected file without force/admin release. Hard-lock availability is controlled by the installed Diversion service/plan. |

Status/inspect exposes repository/workspace/branch IDs, cloud and local base commits,
sync readiness and changed paths. Added/modified/deleted records are mapped directly;
a recognized two-path rename becomes old-deleted/new-added. Status is a cloud
workspace comparison; an unsynced agent can still have pending local changes.
An incomplete sync state is shown explicitly and refuses mutations.

## Parsing and guards

The public CLI reference documents text output, not a JSON/porcelain option. The
provider parses the documented identity lines from
[`status --nowait --no-limit`](https://docs.diversion.dev/cmd-ref/status) and
[`diff --name-status --color never`](https://docs.diversion.dev/cmd-ref/diff).
It never invents an undocumented JSON flag. Unsupported identity, quoted/ambiguous
path/rename formats, invalid UTF-8, duplicate records, excessive output and
incomplete responses fail explicitly instead of becoming a clean project.
Real-client format/version compatibility remains to be established.

Mutation preflight requires matching local/cloud base commits and no reported
pause, transfer, queued events, offline state, conflict or error. Update/checkout
also require an empty change summary. A second status checks repository, workspace,
branch and base identities before dispatch. Concurrent daemon/server/filesystem
changes are not an atomic transaction; the client's own checks remain authoritative.
Unrecognized/omitted conflict states and other client versions need runtime
acceptance rather than a claim of complete conflict coverage.

The mapped root must contain a local, nonsymlink `.diversion/` directory. Parent
traversal, administrative paths, symbolic links and nested-workspace file targets
are rejected. Paths remain inside the CyCode workspace. File selectors use `./`
prefixes to avoid option interpretation; pattern metacharacters, control characters
and ambiguous quoting are rejected rather than expanded. Branch/ref forms use
bounded simple ASCII names; existing Unicode branch names can be displayed.
Tracked symlinks, unusual filenames and submodule-specific editing remain outside
this provider's contract.

The shared native process owner receives argv vectors, fixed cwd and color hints
(`NO_COLOR`, `CLICOLOR`, `TERM`). Diff explicitly requests `--color never`. Only SGR
color sequences are stripped from read metadata; other control sequences fail.
Each process has a 512 KiB output cap. Status/diff/history/annotation reads have
30-second deadlines and final mutations 180 seconds. Generic SDK transport allows
600 seconds for sequential preflight and execution. No undocumented exit code is
converted to success. A timeout stops the CLI waiter; it cannot roll back or stop
work already accepted by the independent Diversion agent/cloud service.

Common native host guards refuse mutations with dirty/open disk-conflicted buffers
and reconcile clean documents afterward while retaining concurrent editor changes.
Provider disable never falls back to Git silently. Generic MCP calls use
`vcs_provider_action` with its existing plugin/process/workspace/VCS permissions;
typed SDK Git operations remain Git-only. See [Plugins](Plugins.md).

Reference contracts:
[commit](https://docs.diversion.dev/cmd-ref/commit),
[branch](https://docs.diversion.dev/cmd-ref/branch),
[checkout](https://docs.diversion.dev/cmd-ref/checkout),
[reset](https://docs.diversion.dev/cmd-ref/reset),
[update](https://docs.diversion.dev/cmd-ref/update),
[annotations](https://docs.diversion.dev/cmd-ref/annotate),
[locks](https://docs.diversion.dev/cmd-ref/lock),
[syncing](https://docs.diversion.dev/concepts/syncing) and
[conflicts](https://docs.diversion.dev/concepts/conflicts).

## Validation and remaining scope

Windows production compilation and MSVC `/Zs` source checks pass. The opt-in native
regression source specifies fake-runner identities, literal/Unicode operands,
sync/merge/error/partial-output guards, changed preflight identities, clean update/
checkout, commit/reset/lock scope, deadlines and missing tools. It also specifies
actual production-DLL registration, mapping, dirty-buffer refusal and disable
behavior. The existing MCP inventory/permission assertions include Diversion.

These native/MCP/GUI regressions are **not executed** during the current Kaspersky
interruption. No `dv` process, login, cloud request, app window or test executable
was launched. See [Validation](Validation.md) for actual checks. Client/agent/service
versions, authentication, hooks, text formats, branches, conflicts, locks and large
repositories remain runtime-unverified; Linux/macOS and clean installation have
not been tested. This is an implemented CLI integration, not full Diversion-client
or Rider parity.

Further work includes clone/init, workspace/sync preference management,
shelves/tags/reviews, merge/cherry-pick/conflict resolution, structured history,
submodules, richer lock views and cancellable VCS jobs. Future providers, including
the owner's planned revision-control system, use the same generic extension seam:
see [Custom revision-control providers](CustomVersionControl.md).

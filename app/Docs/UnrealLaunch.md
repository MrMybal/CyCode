# Unreal editor, game and server launch profiles

The Unreal Engine panel prepares the same persistent run/debug profile used by
CyCode's Debugger and native terminals. Preparation reads project/engine metadata
and file existence; it does not compile, save or start anything. It supports
installed UE4 and UE5 editor executables for the current host platform. Windows
production compilation and schema checks are performed locally; actual Unreal
launch, debug, GUI and Linux/macOS acceptance remain pending during the current
Kaspersky suspension of native execution.

## Desktop workflow

1. Open the `.uproject` or matching `.sln`, select its installed engine and current
   host platform in Unreal Engine. Choose Development or DebugGame.
2. Expand **Unreal launch profile**. Select editor, standalone game or uncooked
   server. Game/server use the installed editor executable and uncooked content,
   rather than a cooked distribution executable.
3. Optionally enter an existing map such as `/Game/Maps/Main`, without `.umap`.
   The file must exist below this project's `Content` directory. Empty uses
   Unreal's project default. Plugin and `/Engine` maps, asset object references,
   URLs and arbitrary console commands are outside this typed preparation.
4. Game mode offers windowed/fullscreen resolution, and **Listen server** for
   an explicit map. Listen/server modes use the chosen port. **Unreal log
   window** is opt-in. DebugGame selects the configuration-specific editor executable (for example,
   `UnrealEditor-Win64-DebugGame.exe`); a missing variant is reported rather than
   launching Development. It must already be installed/built.
5. Building before debugging requires a discovered target whose name ends in
   `Editor`, selected above. Disable this option for content-only projects. This
   is a name/discovery check, not execution or evaluation of `Target.cs` rules.
6. Preview and inspect the executable and each literal argument. Apply with
   **Use this Unreal launch profile**. This saves `.cycode/launch.json` through
   the common debugger and opens its panel. It does not start a process.
7. Use existing **Run without debugging** to start the profile in an owned native
   terminal without compilation, or **Debug / Continue** for the shared DAP
   service and optional build-before-debug workflow. Compilation uses the
   project, engine, Editor target, platform and configuration stored in the
   launch profile, independently of later toolbar changes. Select/install a compatible native
   adapter in the debugger; preparation does not install one. The bundled Unreal
   plugin also exposes an explicit CodeLLDB adapter entry.

The common saved profile retains its environment, watches, terminal choice and
other unrelated fields when the generated patch is applied. Generated fields
select the builtin native adapter and clear previous adapter arguments/options,
avoiding inheritance of a Java/.NET or unrelated plugin launch configuration.
The saved executable/arguments and `unrealLaunch` binding survive restart. The
binding contains the observed Unreal `profile` and normalized launch `request`.
The preparation form restores its mode/map/network/window settings from that
binding. **Load bound Unreal build settings** explicitly restores the build
controls and refreshes native inspection; it does not run a build or editor.
There is still one persistent launch profile store.
Top-level `stopOnEntry` and `buildBeforeLaunch` remain authoritative when changed
after preparation; validation uses those current values over the stored request.

Program, working directory, arguments and native runtime stay bound while the
profile is linked. Regenerate through the Unreal panel to change them, or use
**Detach Unreal launch settings** before manual editing. This explicitly saves
`unrealLaunch=null` when the manual profile is next applied/launched. Environment,
watches, compatible adapter selection, stop-on-entry and the build preference
remain editable. The native owner also rejects mismatched bound fields through
MCP/plugins, before a configuration write or process dispatch. Missing engine,
map, configuration-specific editor or required Editor target is reported.

Legacy/manual profiles without the binding keep their previous workflow and
load with a null binding. Standalone native Debugger integrations that provide
no Unreal validation callback can continue using unbound profiles; a bound
profile requires the common Engine's validator before it can be saved/launched.

Previews are discarded when their Unreal inputs or manual debugger draft change.
Saving compares `expectedProfile` with the complete current native profile before
updating it. A mismatch produces `DebugProfileConflict`; disk baseline/atomic-save
guards remain in force. Updating a watch alone does not reload and overwrite
unrelated unsaved profile fields in the desktop.

## Compilation before debugging

The desktop saves/reconciles documents, captures the complete launch parameters,
manual draft, observed saved profile, open workspace document identities/revisions
and build operation ID, then dispatches the existing cancellable `unreal.build`
owner with the bound five-field build context. Unreal building also rejects dirty
or conflicted native workspace buffers before starting a tool.

After compilation, automatic DAP launch requires a confirmed exit zero with no
cancellation, timeout, truncated output or inspection/receipt uncertainty, plus
an available result whose observed Unreal profile matches the bound context.
The result must also contain a complete, ready assessment of that context's
ordinary Editor outputs. Actual native run/launch rechecks those outputs before
dispatch, while configuring a profile remains possible before compilation. See
[Unreal output inspection](UnrealArtifacts.md) for receipt/module/file checks,
limits, content-only behavior and excluded dependency/ABI coverage.
The build ID, manual draft, saved profile and captured open document identities/
revisions must still match. Dirty/conflicted documents, saved source edits during
the build, or closing/reopening a captured document stop automatic launch. Opening
an additional clean document does not by itself stop it. No retry is automatic.

The captured parameters, rather than newly edited fields, are sent to the shared
debugger. `expectedProfile` is rechecked during queued configuration saving and
removed before the native launch request; it is not forwarded to a DAP adapter.
Run without debugging still does not compile. Existing terminal/DAP ownership,
cancellation and shortcuts remain in force, including in mixed project workspaces.

These guards cover the launch/profile context and captured live documents. They
are not a snapshot of every closed source file, engine binary or build input, and
do not make compile/save/launch one atomic filesystem or tool transaction.

The preview generates a profile; it does not freeze execution inputs. It does
not reserve binaries, maps or content, check listening port availability, validate
module compatibility, or promise successful editor startup. Tools/project code
can change before an actual launch.

## MCP and plugins

`unreal_launch_profile` maps to `unreal.launchProfile` in the same native Engine.
Optional parameters are existing Unreal project/engine/target/platform/configuration
fields plus:

| Field | Values/default |
| --- | --- |
| `mode` | `editor` (default), `game`, `server` |
| `map` | Empty or existing `/Game/...` package; same lexical validation as cook/package |
| `listen` | `false`; true requires game mode and an explicit map |
| `log` | `false`; requests Unreal's separate log window when enabled |
| `windowed` | `true` for game; ignored/normalized false for editor/server |
| `width`, `height` | 1280 × 720; width 320–16384, height 200–16384 |
| `port` | 7777; 1–65535, emitted for server/listen only |
| `stopOnEntry` | `false`; used by the DAP launch |
| `buildBeforeLaunch` | `true`; desktop build-before-debug preference |

Response fields are normalized `request`, a `settings` patch, observed
`unrealProfile`, saved `expectedProfile`, and explicit `startsProcess=false`,
`savesProfile=false`, `usesUncookedContent=true`. Preparation requires
`process.execute`, matching `debug_profile`, because the saved profile can include
private environment variables. The desktop does not dump that snapshot into build
output. The SDK workspace-mutation exclusive group is not acquired for preparation.

Send `settings` plus `expectedProfile` to `debug_configure`; its existing
`process.execute` and `workspace.write` permissions apply. Then use `debug_run` or
`debug_launch`. These SDK/native launch operations do not perform the desktop's
optional compilation step: explicitly build through `unreal_build` and inspect
its actual result first when needed.

Prepared settings now require a non-null `unrealLaunch` binding in their result.
The ordinary debug profile/configure/launch/run schemas accept that binding or
null, and retain compatibility with an absent field in legacy profiles. Both
native configure and actual run/launch revalidate bound executable/cwd/argv and
runtime through the Unreal owner. SDK `unreal_build` uses explicit parameters;
SDK `debug_launch`/`debug_run` still do not compile automatically. Clients can
read the persisted binding via `debug_profile` to coordinate their own workflow.
Persist detachment through `debug_configure`; a null override passed only to an
SDK run/launch is a one-off override, consistent with other launch fields.

ABI2 exports preparation with the same `process.execute` permission; read-only
provider callbacks may prepare a profile but cannot save or launch it. Bundled
Unreal provider 1.5.0 retains `launchProfile` and the common binding checks; ABI version stays 2.

Existing attach-by-PID remains in the shared debugger, with its normal detach/
termination semantics. This addition does not implement Blueprint debugging,
PIE control, Live Coding, editor RPC, multiplayer session orchestration, remote
client connection profiles, or packaged game/server discovery. Those need their
own engine integration and runtime acceptance.

Argument semantics follow Epic's primary [command-line guide](https://dev.epicgames.com/documentation/unreal-engine/command-line-arguments-in-unreal-engine)
and [argument reference](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-command-line-arguments-reference).
Configuration-specific executable naming was also inspected read-only in installed
UE 5.3 UBT, LaunchEngineLoop, App and ModuleManifest sources and editor filenames.
No Epic source or engine binaries are copied or distributed by this addition.

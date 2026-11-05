# Reviewed Unreal cooking and packaging

**Unreal Engine > Cook and package Unreal content** adds cook-only and
build/cook/stage/package/archive actions. Desktop, CyTools/MCP and ABI2 plugins
share the native Unreal owner and existing cancellable build service. The
packaged game is not launched by the submitted command.

## Configure and review

Open the actual `.uproject`/solution and select the matching installed Unreal
Engine 5. Automation requires its precompiled AutomationTool and editor commandlet
binary, a discoverable .NET host and a project Target.cs entry for the requested
game target. Names ending in Editor are rejected. Rule evaluation, actual target
type, supported SDKs and engine/project compatibility are assessed by Unreal when
the command runs; the planner does not execute C# to infer them. Blueprint-only
projects without project target rules and mobile/server/console workflows remain
outside this initial service.

Choose **Cook content** or **Package and archive**, a game target, platform and
configuration. Platform/configuration reuse the Unreal controls above the new
section. Desktop choices are Win64, Linux, LinuxArm64 and Mac; availability still
requires an appropriate engine/toolchain. Select whether to build game code.
Packaging exposes Pak, IoStore and compression; IoStore/compression require Pak.
Unchecked IoStore/compression produce explicit skipiostore/ForceUncompressed
arguments so project defaults do not silently enable those requests. Unreal's
platform/package rules remain authoritative for the resulting container layout.

Enter one existing `/Game/Folder/Map` package per line without `.umap`. Empty uses
the project's packaging defaults. There are at most 64 names, 512 UTF-8 bytes each,
and an 8,000-byte combined map argument budget. Options, plus separators, URLs,
relative traversal, object/file extensions and duplicate/case-ambiguous names are
refused. Explicit plugin/Engine map mounts are not mapped by this initial selector.

**Preview Unreal automation** resolves canonical project/engine paths and shows
the exact argv, cwd and managed output/archive locations. No tool is executed and
no output directory is created. Native previews are per Engine instance, capped
at 16 and valid for ten minutes. **Cancel automation preview** drops the desktop
review; native unused entries expire. Reopening a core loses its issued previews.

The deadline is 1..86,400 seconds, default 3,600. Run must supply exactly the value
reviewed with the plan. Changing controls invalidates the desktop preview. Late
preview replies for changed controls are discarded.

## Run, cancellation and retained files

**Run reviewed Unreal automation** requires the issued planId/revision and saved,
reconciled workspace documents. Native preflight rechecks descriptor/target rule
content, explicit map file stamps, engine version, tool/editor stamps,
workspace/engine directory identities and output vacancy. Binary/map stamps use
path/size/modification metadata; they are not executable signatures or complete
content hashes. This is an argument/context review, not a frozen snapshot of all
sources, assets, configuration or caches. Builds use current project inputs.

The adapter starts the selected precompiled AutomationTool directly through its
resolved .NET host/apphost with separate argv entries. It reuses the existing
Unreal managed-tool discovery; legacy and newer host platform folder aliases are
recognized. It does not copy the engine's batch scripts or build UAT itself.
Missing precompiled tools must be built/installed through Unreal first. Runtime
environment includes the selected .NET root/host and the managed UAT log folder.

The command requests BuildCookRun, unattended UTF-8 output and no Perforce/editor
compilation. Package adds stage/package/archive, explicit staging/archive paths
and reviewed container options. It does not request clean, deploy, run, upload or
SDK installation. Project rules, custom scripts, native tools and packaging
settings remain trusted executable code; signing/encryption or external effects
configured by them are not sandboxed away. Ordinary Unreal Intermediate, Saved,
cook/build/DDC and engine/user caches can change.

Each attempt reserves a new `.cycode/unreal-automation/uat-...` directory through
exclusive creation. Existing managed output is never deleted or reused. Packaging
staging/archive live below that directory; cook uses Unreal's normal project
cooked-data paths. The same directory retains:

- intent.json: the reviewed plan before dispatch, not proof of completion;
- output.log: up to 8 MiB of raw captured process output;
- logs/: UAT's own logs, whose sizes and additional engine/child log locations
  remain controlled by Unreal;
- result.json: the adapter's actual process/postflight result when writable;
- stage/ and archive/ for package actions, including partial results on failure.

The preview permits one execution attempt after preflight. Errors, cancellation,
timeouts and truncated output do not delete artifacts or automatically retry.
Build > Cancel build, the palette, build_status/cancel and SDK cancellation use
the existing correlated operation ID and owned process tree. The reservation stays
held until native work stops. Late owner/SDK cancellation can augment the returned
job status after the adapter result was recorded; inspect the job as well as the
retained files. Inspect/preview fail busy rather than waiting behind a long Unreal
operation. No cross-instance/global project freeze is claimed.

**Copy automation output path** and **Open automation log** expose the retained
results. They are persistent workspace data, not temporary SDK task artifacts;
SDK session cleanup does not own or remove them. There is no automatic editor
Undo/rollback, distribution, package launch or deletion workflow.

## Actual outcomes and contracts

The result retains exitCode/output/timedOut/truncated/cancelled. Invalid or cut
UTF-8 in the returned text is replaced with outputRepaired=true; output.log keeps
the original bytes. Postflight checks managed directory identity and looks for an
ordinary archive file, bounded to 2,000 entries/five seconds. A zero exit without
an observed archive carries inspectionError. Observing a file does not validate
the full archive, platform binary, installation or game behavior. receiptError
reports failure to persist result.json without hiding the actual process status.
The UI shows a warning for unconfirmed results instead of a success toast.

| SDK operation | Native / ABI2 host method | Effect |
|---|---|---|
| unreal_automation_plan | unreal.automationPlan | Typed cook/package controls; no process/output creation |
| unreal_automation_run | unreal.automationRun | Issued planId, expectedRevision, exact timeoutSeconds; one cancellable attempt |

Run requires process.execute and workspace.write, shares the SDK workspace
mutation group and the native build owner, and publishes common build events.
The bridge generates an operation ID, correlates build.cancel and extends its wait
to the chosen deadline plus native postflight allowance. Native host permission
checks also reject writes from read-only plugin callbacks. The bundled Unreal
provider 1.5.0 advertises both actions with the matching permissions. No C ABI,
dependency, executable target or CyUI module is added. The 256 MiB SDK reservation
is for bridge/metadata work; external Unreal tool memory/disk use is project-owned
and has not been measured here.

## Validation and remaining scope

Windows production compilation and MSVC /Zs, real CyToolsCore manifest loading and
Draft 2020-12 schema/data validation are distinct from actual Unreal acceptance.
The existing opt-in solution fixture adds pure phase/map/container/deadline cases,
nonwriting preview, cancelled-before-dispatch, stale-rule rejection and occupied
output retention. These native assertions have not run during the existing
Kaspersky suspension; no test/native fixture process, UAT, UBT, commandlet or
packaged game was launched for this milestone.

Argument/runtime setup was checked against the installed UE 5.3 primary source
files RunUAT, ProjectParams and CopyBuildToStagingDirectory. Actual UE versions,
tool/child process trees, logging/caches/artifacts/cancellation, SDK/plugin/GUI,
keyboard/themes/DPI and Linux/macOS remain unverified. Live Coding, Unreal Editor
automation, asset/Blueprint refactoring, advanced target/platform/signing/profile
management, device deployment and complete package validation remain separate work.

The phase model follows Epic's [build operations documentation](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine).

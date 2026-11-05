# Test, package and workflow presets

View > CMake preset actions opens the secondary panel. Build and the command
palette also expose CMake test/package/workflow presets. Select a workspace
source directory and preset, preview its steps, then run the reviewed preset.
Inspection does not invoke CMake, CTest, CPack or project scripts.
The action selection is transient; panel visibility uses the existing workspace
session metadata, while the ordinary saved build profile remains separate.

The preview shows the linked configure preset, binary directory, configurations,
ordered steps, declared output paths and process deadline. CPack retains multiple
configurations. Public paths remain workspace-relative. Ordinary configure/build
selectors and the successful saved build profile keep their existing ownership.

| Kind | Native command | Prerequisite |
| --- | --- | --- |
| Test | ctest --preset NAME | Matching configured binary tree; command needs CMake/CTest 3.20+ |
| Package | cpack --preset NAME | Matching configured binary tree and project packaging setup; CMake/CPack 3.25+ |
| Workflow | cmake --workflow --preset NAME | Configure first, then build/test/package steps linked to that same configure preset; CMake 3.25+ |

The installed tools must also support the file schema and chosen options.
Conditions, filters, dependencies, generators and project/package commands remain
owned by the real tools. References: official [presets](https://cmake.org/cmake/help/v4.1/manual/cmake-presets.7.html),
[workflows](https://cmake.org/cmake/help/v4.1/manual/cmake.1.html#run-a-workflow-preset),
[CTest](https://cmake.org/cmake/help/v4.1/manual/ctest.1.html) and
[CPack](https://cmake.org/cmake/help/v4.1/manual/cpack.1.html).

## Metadata and review

The existing native reader now loads all five preset families. It retains hidden
bases, local includes, earlier-parent inheritance, environment/cache/output/
CPack-variable maps and defining-file macro origins. Action environments insert
configure variables after inherited action variables and before explicit action
variables. Workflows have no inheritance/hidden mode; all step references must be
visible from their defining file and use the initial configure preset.

Includes, binary roots, explicit CTest log/JUnit paths, CPack config paths and
declared package directories pass the workspace resolver. In-source/ancestor
binary roots and caches belonging to another source tree are refused. Complex
conditional availability remains a CMake execution check; the selector does not
evaluate every condition or project-defined output.

Execution requires expectedRevision from the preview. The token covers loaded
preset contents, root-file presence and the normalized plan. New/changed/deleted
root or included preset files require another preview. Snapshots are checked
before and after execution. This is an optimistic concurrency check, not
authentication or a freeze of sources, process environment and installed tools.
Filesystem races are not a transaction.

Bounds: 64 files / 8 MiB source metadata, 512 entries per family, 64 JSON/
inheritance levels, 256 KiB per resolved preset / 16 MiB combined, 16 MiB public
catalogue, 2 MiB plans, 64 workflow steps, 64 CPack configurations and 32 KiB path
expansion. Invalid paths, steps and limits produce explicit errors.

## Execution and cancellation

The common BuildOperations reservation owns build.presetRun alongside ordinary
CMake/MSBuild/Unreal work. The adapter uses argv and the source directory as cwd;
CTest/CPack infer their binary tree from the native preset. Companion tools are
selected beside detected CMake when present, then through the usual tool resolver.

Workspace buffers must be saved. The GUI retains the preview if saving is
cancelled. Cancel build, app closing and SDK cancellation signal the same
correlated operation ID; process-tree ownership and the reservation remain until
native work stops. The main deadline covers the whole workflow: 180 seconds for
direct calls, the configured desktop value in the GUI, or a 1–86,400 second
override. Tool discovery retains separate bounded probes. Output is capped at 8 MiB.

No fresh/force-clean option is added. Failure/cancellation can leave compiled
files, reports, packages and completed steps. There is no automatic rollback,
deletion or invented per-step success parsed from console text. Successful
workflows can refresh existing File API/C++ analysis for the last explicit build
configuration without replacing the saved profile. A later metadata/snapshot
failure is returned as inspectionError separately from the actual process result.

Project/test/package scripts are trusted executable code. Guards cover known
metadata fields, not every output a script or generator might create. Configured
CTest's structured test tree remains separate; this view returns preset output,
not a fabricated per-test tree or installer artifact browser.

## Shared SDK and plugins

build_preset_plan accepts source (default "."), kind (test/package/workflow) and
preset; it is read-only. build_preset_run additionally requires expectedRevision,
accepts timeoutSeconds and requires process.execute plus the existing SDK
workspace-mutations group. Callers cannot supply arbitrary commands/arguments.
The real SDK owns jobs, identities, permissions and cancellation.

Native plugins export build.presetPlan/run through the existing JSON host request
and process permission. No C ABI layout, CyUI module or Python process/parser
implementation is added. Paths remain persistent workspace references.

## Acceptance

See [Validation](Validation.md) for completed checks. New parser/plan assertions
extend the existing opt-in cycode_build_tests target without another executable.
They cover workflow order/linking, inherited output/environment behavior,
hidden/escaped paths, stale revisions and newly appearing user-preset files.

Native metadata fixtures, actual test/package/workflow commands, cancellation,
File API refresh, SDK/plugin invocation, GUI input/scroll/focus, themes/DPI and
Linux/macOS remain unexecuted during the Kaspersky interruption. Source/schema
checks and production compilation do not establish runtime acceptance.

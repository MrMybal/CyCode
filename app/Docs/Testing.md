# Local verification and native tests

Header/source and include navigation have 106 compiler-evaluated helper contracts
on the actual native constexpr owners. The 27 additions cover inline comments,
logical LF/CRLF components, spliced keywords, physical header offsets, conservative
refusals, forced choice, file-page revision/generation pairs and help anchors.
MSVC /Zs and Clang syntax-only checks cover eight affected translation units.
The fixture also contains a real compiler include directive split across lines
with inline comments. Native bodies specify continued Unicode includes with a
caret on the first line and a header range on the third, in addition to the prior
index/suffix/case/scope/dirty-buffer/source-guard contracts. These bodies are
syntax checked only, not linked or executed.

`python -B app/tests/test_navigation_contracts.py` has six JSON-data-only checks
of guarded source/caret inputs, explicit cache observations, continued physical
ranges and required positive file-page generations. Together with the five
snippet checks below, eleven tests run without an SDK/application/native runtime.
JSON validation does not establish filesystem, callback, cache contention or
permission behavior. Actual clipboard, refresh/close race and configurable modal
help-keyboard acceptance remain pending. DOM tests exercise the local manual's
matching EN/FR stable anchors; they do not launch a browser or prove native help
handoff, rendering or keyboard behavior.

`python -B app/tests/test_snippet_contracts.py` runs five JSON-data-only tests:
generated input/output parity, disabled/unreadable zero-generation states,
save revision/fixed-location validation and permission/scheduler declarations.
It imports schema constants, without tool.py, the SDK runtime or native processes.
This does not establish native permission, hashing or file-lock behavior.

Selection capture adds fourteen compiler-evaluated contracts on the actual shared
literal-body helper (143 combined snippet assertions): dollar/backslash/brace
escaping, Unicode/newline retention, empty/NUL refusal and plain/escaped 8 KiB
boundaries including the final caret. Native fixtures additionally specify
noncreating drafts, unchanged existing definitions/source files, unique names,
explicit save, invalid/full catalogue refusal, unsaved Verse capture and stale or
split-surrogate selection rejection. They are syntax-checked, not linked or run.
The fifth Python test checks the read-only draft schemas and metadata as data.

Personal snippet helpers add twelve compiler-evaluated revision/byte-limit
assertions to the existing 117 (129 total). Native fixture bodies specify disabled
hosts, noncreating reads, cross-instance revisions, retained-lock refusal,
malformed JSON repair, invalid UTF-8/oversize preservation, two-workspace queries,
stale-session refusal and existing plan/apply/Undo behavior. The extension fixture
adds denial of settings.write for a workspace-only plugin. These bodies remain
unlinked/unexecuted. No native target is added. Native hash/file/locking/race,
GUI keyboard/dirty-dialog/Undo and Linux/macOS acceptance remain unverified while
the antivirus suspension is in effect.

Plugin snippets add eight compiler-evaluated boundary assertions on the actual
registry count/byte policy, bringing the combined snippet helper total to 117.
Native fixture bodies specify descriptor/owner validation, excluded scopes,
workspace-first merging, duplicate provenance and global truncation, summary-only
inventory, same-thread host queries, disable invalidation and Unreal field rendering.
The existing completion/extension/Unreal sources are syntax checked only; no new
native target is added and these fixtures are not linked or executed. Actual plugin
loading, callbacks, lifecycle races, edit/Undo and graphical/platform acceptance
remain unverified during the antivirus suspension.

Selection wrapping adds 21 compiler-evaluated assertions on the actual production
CompletionSelection policy: forward/reverse caret endpoints, empty/multiline and
Unicode byte ranges, invalid anchors/order/bounds, and the 8192-byte limit. The
combined variable/format/catalogue/selection helpers now have 109 assertions under
ordinary compiler budgets. The compiler-only pass covers fifteen translation units.
Native opt-in source bodies also specify literal escaping, field metadata, unused
defaults, integer/surrogate guards, multiline layout, reverse selections, no-LSP
Verse preparation/preview/apply/undo and stale catalogue/session refusal. These
bodies and the Ctrl+Alt+J keymap fixture are not linked or run. Native thread races,
real keyboard/GUI/Undo, regex matching and Linux/macOS remain unverified during the
antivirus suspension. The SDK schema remains 228 operations with optional selection;
source/data and DOM checks cannot prove runtime or graphical acceptance.

The project-snippet catalogue adds 21 compiler-evaluated assertions on the actual
prefix and scope policy: exact/partial/mismatched/case-sensitive prefixes, retained
suffixes, punctuation, Unicode bytes, empty/invalid positions, scope tokens and
limits. MSVC /Zs and Clang syntax-only checks include the native catalogue, Engine,
completion, plugin/command owners, UI and existing opt-in completion/keymap fixtures.
Native source bodies specify parser/schema/size/depth limits, scope filtering,
unopened-document refusal, no-LSP Verse issuance, prefix/suffix preservation,
transformed fields, preview/apply/undo and stale catalogue/disk rejection. They
are not linked or run. No new executable or native test target is introduced.
Real worker races, parser/file I/O, SDK/plugin transport, keymap, Escape, focus,
palette and widget/Undo/platform acceptance remain unexecuted. See
[Project snippets](SnippetCatalogue.md).

The final compiler-only pass also reevaluates the previous 67 variable/format
assertions: 88 compile-time contracts at that milestone, before the 21 selection
contracts above, with no raised constexpr budget.
All 228 SDK input/output schemas are checked as data with the standard JSON Schema
validator; the new route, read-only permissions, required revision and negative
position refusal are verified without creating a runtime, MCP or native process.
The offline manual DOM fixture checks the new EN/FR catalogue/API routes and
local search, alongside the existing six themes and keyboard/media controls.

Variable-transform checks add 39 compiler-evaluated assertions on the actual
production scanner and capture-format parser. They cover captures/conditionals,
nested/unused branches, case dispatch, escapes, Unicode literals, invalid formats,
flags and scanner/capture limits. Together with the existing 28 variable assertions,
67 compile-time contracts pass MSVC /Zs and Clang syntax-only checks with normal
Windows production flags. The case callback assertion verifies dispatch only;
it does not execute utf8proc or PCRE2.
CompletionTransforms.cpp and the new completion_transform_tests.cpp are checked
alongside the existing completion/engine/UI units. Native source fixtures specify
real matching, Unicode casing, empty global matches, limits, deferred popup text,
transformed initial fields and filename/import/apply/undo through issued proposals.
These bodies are not linked or executed. The fixture joins the existing opt-in
completion target; no extra native test executable is introduced. Actual regex
timing/cancellation, PCRE2/utf8proc behavior, GUI input and Linux/macOS remain to
validate after the antivirus issue is resolved. See [Completion variables](CompletionVariables.md).

Completion-variable substitution adds 28 compiler-evaluated assertions on the
actual preprocessing helper: selected/unused defaults, numbered-field passthrough,
choices, literal escaping, Unicode, malformed/unsupported forms, 8 KiB raw/escaped
budgets and depth/node boundaries. MSVC /Zs and Clang syntax-only checks use normal
Windows production flags; no raised constexpr budget is required. Existing native
completion/service fixtures specify real context values, imports/byte ranges,
literal filenames, large lines and the ordinary apply/undo guards, but their bodies
are unexecuted. No extra native target or duplicate snippet renderer is added.
See [Completion variables](CompletionVariables.md).

Toolchain reporting adds six pure Python checks for Clang/MSVC/CMake context,
bounded output, unknown failures and bounded large-log reads. Together with the
five in-memory packaging checks they execute without a native fixture. The native
core suite specifies executable-alias preservation, missing targets and POSIX
permission filtering, PATH/hint order, directory aliases and invocation identity.
The existing build suite checks actual detected C++ paths. Their MSVC and Clang
syntax-only checks use Windows headers/ABI; fixture bodies were not run. No POSIX
runtime execution is claimed. See [Toolchain reliability](ToolchainReliability.md).

Detailed update history adds twenty actual attention/phase/count/outcome assertions,
bringing updater policy checks to 115 compiler-evaluated assertions. Opt-in bodies
specify discovery beyond twelve metadata rows, no created locks, sorting/unknown
times, selected/full report exclusions, foreign/oversized/invalid records,
cancellation and budgets. A GUI fixture specifies test-session exclusion. Bodies
remain syntax-only under the antivirus suspension; no native history/helper/GUI
fixture is generated or run. See [History](UpdateHistory.md) for pending acceptance.

Recovery controls add sixteen assertions on production phase-local progress and
stable phase codes, bringing installation/recovery plus download policies to 95
compiler-evaluated assertions. Source-only fixtures specify typed pre-cancellation,
progress reuse/foreign-plan rejection, exact fresh/reused copy bytes, result receipt
states and report exclusions. The helper manifest/EN-FR catalogue/project references
are checked as data; production PE resources are read without executing them.
Stop/close timing, clipboard, blocked I/O, cancellation after file moves, receipt
failures and graphical theme/DPI/restart acceptance are still unexecuted. No native
fixture is built or run during the antivirus suspension. See
[Recovery controls](UpdateRecovery.md).

Space/progress checks add nine constexpr headroom assertions and two multi-case
meter contracts to the existing nineteen HTTP assertions. They evaluate actual
production code in MSVC /Zs: resumed/full/complete offsets, invalid bounds, rate
sampling/smoothing/stalls, reset counters/clocks and ETA bounds. A source-only
native fixture describes caller-space observation and retained-file behavior.
Real low-space/quota/write-failure and network/GUI acceptance remains pending;
no disk-filling fixture, native test executable or app is run. See
[Download observations](UpdateDownloads.md).

Update-retry source contracts add exact/most-advanced checkpoint selection, fresh
noninspection and file retention, cancelled/invalid modes and typed response errors.
Module source fixtures inject completed inspection futures to specify preserved
feedback, quiet refreshed candidates, replaced digests and failed/cancelled local
refreshes; they never invoke a real transfer. Smoke/test-session source guards cover
fresh downloads and post-attempt scanning. These fixture bodies are syntax checked
only, not linked or executed. Existing nineteen constexpr response assertions are
still compiler-evaluated. See [Download retry](UpdateDownloads.md).

Download-resume verification evaluates nineteen static assertions on the actual
production constexpr response policy through MSVC /Zs, without native execution.
Additional opt-in source contracts cover checkpoint/asset identity, readonly
inspection, foreign/rejected/unknown metadata, limits and interrupted staging.
Actual transfer, WinHTTP headers, locking, cancellation and GUI acceptance remain
pending; see [Download resumption](UpdateDownloads.md).

Cache-maintenance source fixtures cover owned/legacy/current protection, read-only
review, active leases, stale/pending/cancelled actions, reversible moves, receipt and
destination conflicts, partial restore, and retained metadata/backups/user data.
Core/module source checks only; no native fixture or maintenance operation is run.
See [Update storage](UpdateStorage.md).

Guided recovery sources specify immutable plan preparation, read-only/conflicted
review, exact confirmation guards, bounded/non-overwriting restoration, retained
backups/user files, cancelled/stale actions, instance exclusion, partial restoration
resume and legacy refusal. Existing opt-in core sources only; no native execution
under the Kaspersky suspension. See [Update recovery](UpdateRecovery.md).

Update-cache follow-up sources specify restart discovery, bound outcome history,
retryable archives, cancellation, corrupted helpers/archives, foreign metadata,
pending markers, outside staging paths, retained downloads and changed release
digests. These bodies are syntax-checked only under the existing native-run
suspension; no SDK Runtime or executable fixture is imported or run.

Application-update regression sources join the existing core/module targets,
without another test executable. MSVC /Zs checks the native release, transport,
package, helper, UI and integration/regression sources. No fixture/helper/app is
launched. Source cases specify version/asset/digest/path rejection, locked-file
rollback, user-data retention, repeated-version refusal, installation lease exclusion,
skip/manual proposal behavior, test-session network guards and cancelled close
intent. Five Python authoring tests execute only in-memory ZIP/JSON/hash/PE-header data;
no fake EXE file is written or run. The existing CI matrix includes this data-only
suite, but has not run remotely. Production build and PE resource inspection do
not establish real HTTP/helper/rollback/GUI/platform acceptance. See
[Application updates](Updates.md) and [Validation](Validation.md).

Occurrence regression sources extend existing semantic, command, module and
language-lifecycle fixtures with no new executable target. MSVC /Zs checks affected
production/regression sources only; fixture bodies are not linked or run. Cases
specify actual capability negotiation, source/backend lifetime and unrelated-edit
handling, normalization/kinds/Unicode/budgets, cyclic masked navigation, atomic
selection/source/Undo preservation, independent annotation geometry, native tab/wrap
metrics and clear-during-request. Actual SDK load_manifest and all 227 operation
schemas pass pure bridge/data guards, strict result forms, permissions, status and
EN/FR forwarding checks. No SDK Runtime/native client is imported. Production and
source/schema checks do not establish GUI, server or platform acceptance. See
[Document occurrences](Occurrences.md) and [Validation](Validation.md).

Source-navigation regression sources extend the existing semantic, command,
module and lifecycle fixtures without a new executable target. MSVC /Zs checks
16 distinct affected production/regression sources, including final focused
range/signature/ownership refinements. Sources specify Location/Link/duplicate/
malformed/Unicode/budget cases, cached filtering/keyboard wrap, profile bindings,
capability negotiation, caller identity/epoch, unrelated edits, source edit/close/
replacement/server disable, unsupported methods and late UI tickets. They are
not linked or run. Actual SDK load_manifest and all 226 operation schemas pass
strict per-method outputs/provenance, caller guards, bounds/invalid forms,
permissions, exact facade forwarding and EN/FR data. Native/server/SDK/plugin/GUI/
POSIX acceptance remains pending. See [Source navigation](SourceNavigation.md).

Deferred-symbol regression sources extend those same existing targets: URI-only
versus malformed ranges, opaque data and capability negotiation, issued/unissued
indices, oversized cache entries, stable/cached replies, four-session eviction,
edit/close/server-disable during resolution, changed/missing destinations, and
query/selection reply isolation. MSVC /Zs passes the eight affected production
sources and final seven core/UI/regression sources; no fixture is linked/run.
The actual installed SDK loader/all 223 operation schemas pass complete versus
pending symbol shapes, session/index budgets, invalid forms, permissions, exact
native facade forwarding and EN/FR data. Actual server timing, TTL/cache, keyboard/
GUI/SDK/plugin and POSIX acceptance remains pending. See
[Symbol navigation](SymbolNavigation.md).

Symbol/structure regressions extend existing semantic, module and language
lifecycle fixtures without a new executable target. MSVC /Zs passes 13 affected
core/UI/model/fixture sources plus a final four-source cache/sort check. Sources
specify hierarchy/flat/malformed/duplicate results, bounds, UTF-16, ancestor
filtering/order, precise document lifetime, unrelated edits and late query/panel
replies. The real SDK loader/all 222 schemas check expanded output/input forms,
invalid cases, unchanged permissions, exact forwarding and EN/FR labels.
Fixtures are neither linked nor run. Real server timing/SDK/plugin/GUI/docking/
keyboard/Unicode/themes/DPI and Linux/macOS acceptance remain pending under the
Kaspersky suspension. See [Symbol navigation](SymbolNavigation.md).

Diagnostics/source-link regression sources extend existing build, module and
language lifecycle fixtures. MSVC /Zs checks 17 affected sources, without linking
or executing those fixtures. The actual SDK loader/all schemas validate 222
operations, build_location forms/control-byte rejection/read classification and
identity-bearing language status. Production builds keep BUILD_TESTING=OFF.
Actual parser/filesystem execution, worker timing, SDK/plugin/GUI keyboard/focus,
Unicode/themes/DPI and Linux/macOS checks remain pending under the Kaspersky
suspension. See [Compiler locations](CompilerLocations.md) and
[Diagnostics](DiagnosticNavigation.md).

Three project-output/navigation milestones extend existing build/project/module
fixtures without new executable targets. Sources specify shared journal context/
revision/ID reuse, fragmented/oversized Cargo protocol records, plain text repair,
read-only status/output with dirty buffers, active cancellation ownership, filtered
wrapping/file anchors and Rider/Visual Studio/VS Code bindings. They pass MSVC /Zs
only and are neither linked nor run. Actual SDK loader/all schemas check 222
operations, contextual report shapes, permission/mutation-group isolation, pure
bridge read classification and EN/FR labels. Real process, keyboard, SDK/plugin,
GUI/themes/DPI/concurrency and POSIX acceptance remain pending under the Kaspersky
suspension. See [Project output](ProjectTaskOutput.md) and
[Diagnostics](DiagnosticNavigation.md).

Live build output extends the existing opt-in build fixture without a new target.
Sources specify ID/revision/reused-ID boundaries, thread/token/nested isolation,
retention/byte counts, control/UTF-8 repair, cancellation/failure retention, late
callback rejection and CMake pipe integration. Sources pass MSVC /Zs only; no
fixture is built or executed during the Kaspersky suspension. Actual SDK loader/
all schemas validate 219 operations, including idle/live/conditional/overflow/error
report forms, permissions/no mutation group, pure bridge data and EN/FR labels.
Real process/SDK/plugin/GUI/POSIX acceptance remains pending; see
[Live build output](LiveBuildOutput.md).

Unreal artifact assertions extend the existing solution test target without a
new executable. Sources specify missing/matching Editor receipts, actual target
type/configuration/project/BuildId checks, manifest-listed file absence, mismatched
plugin IDs, invalid filename/path variables/escapes, metadata limits, content-only
scope and unsupported platforms. Continuation assertions reject missing/unready
assessments. These are MSVC /Zs source checks only: do not generate their metadata
binary placeholders or execute fixtures during the Kaspersky suspension. The
actual SDK loader/all 218 schemas and pure bridge data validate report forms,
bounds, outcome uncertainty, permissions/reservation, ABI2 and localized controls.
Production remains BUILD_TESTING=OFF; actual build/load/GUI/POSIX acceptance is pending.

The existing extension fixture also specifies host read access and denied document/
refactoring writes for project inspect, artifacts, launchProfile and automationPlan
callbacks. Its sources are MSVC /Zs checked only; no DLL fixture is built or loaded.

The same opt-in solution fixture now specifies bound-profile persistence/reload,
explicit detachment, invalid program/runtime/request rejection, captured successful
continuation and failures for changed draft/saved profile/build ID/context, cancelled/
timed-out/truncated/uncertain builds and document revisions/identity/conflicts.
Additional clean opens are allowed. These assertions are MSVC /Zs checked only,
not executed. The actual SDK loader/all schemas also check nested binding payloads,
legacy/null profiles and required prepared bindings. Production remains tests OFF;
actual build/debug/persistence/cancellation/GUI and POSIX acceptance is pending.

Unreal launch extends the existing solution test target with pure mode/argv/
listen/map/port/dimension/configuration assertions, metadata-only editor/map
fixtures, content-only/missing-target checks and debug-profile stale-save guards.
These source assertions are syntax-checked, not executed. Do not run them or
create their empty binary placeholders during the current Kaspersky suspension.
Use production BUILD_TESTING=OFF and the real SDK loader/schema checks without
Runtime. Actual editor/game/server/debug/GUI/DPI/keyboard and POSIX acceptance
remains pending; see [Unreal launch](UnrealLaunch.md).

Unreal automation extends the existing opt-in solution fixture, without another
test target. Twelve distinct native model/service/UI/build/plugin/integration/
fixture sources pass MSVC /Zs, with final targeted host/DPI/outcome/ownership
checks. Source assertions specify typed phases/maps/containers/deadlines,
nonwriting preview, cancellation before dispatch, stale rules and occupied-output
preservation; these assertions have not run. Real CyToolsCore loader/all Draft
2020-12 schemas pass for 218 operations, including two new automation contracts,
guards, bounds, uncertain outcomes, permissions/groups, cancellable build owner/
bridge, ABI2 exports, bundled provider and EN/FR controls. Production builds keep
BUILD_TESTING=OFF. Actual UAT/UBT/cook/package/commandlet, tool trees/caches/logs/
artifacts/cancellation/concurrency, SDK Runtime/plugin/GUI/themes/DPI and Linux/
macOS remain unverified during the Kaspersky suspension. See
[Unreal automation](UnrealAutomation.md).

Single-commit cherry-pick/revert extends the existing VCS fixture target without
another executable target. Ten distinct model/service/UI/integration/fixture
sources pass MSVC /Zs, with a targeted final metadata guard check. Source assertions
specify invalid IDs/options, stale/open-document guards, default hook suppression,
real cherry-pick/revert outcomes, resolved continuation and dirty-buffer/index/
disk preservation on quit. These native assertions have not run. Actual SDK
loader/all Draft 2020-12 schemas pass for 214 operations, including three new
replay contracts, action/ID/guard rules, bounds/blockers, uncertain results,
permissions/groups, noncancellable ownership, 1,200-second wait, typed adapter,
three ABI2 exports and EN/FR controls. Actual Git/tools/receipt/filesystem/
concurrency/SDK Runtime/plugin/GUI/themes/DPI/POSIX acceptance remains pending
during the Kaspersky suspension; see [Git replay](GitReplay.md).

Merge completion/quit extends the existing opt-in VCS target, without another
executable target. Ten distinct native model/service/UI/integration/fixture
sources pass MSVC /Zs; targeted final checks cover raw commit parsing, exact-limit
messages and intent-to-add blocking. Fixture assertions specify stale metadata/
index rejection, dirty-buffer/index/disk preservation on quit, metadata guards,
hook failure/default suppression and actual commit parents/tree/index outcomes.
They have not run. Actual CyToolsCore loader/all Draft 2020-12 schemas pass for
211 operations, including four merge contracts, guards, limits, action/message/
hooks rules, permissions/groups, noncancellable ownership, typed adapter, four
ABI2 exports and EN/FR controls. Production Windows builds keep BUILD_TESTING=OFF.
Real Git/hooks/receipt/filesystem/concurrency, SDK Runtime/plugin/GUI/themes/DPI
and Linux/macOS acceptance remain pending during the existing Kaspersky suspension;
see [Merge workflow](GitMergeWorkflow.md).

Working-file synchronization extends the existing VCS/resource fixtures, without
another executable target. Fourteen distinct C++ sources pass MSVC /Zs: binary
file primitives, Resources/Engine/VCS owner, UI/plugin integration and two fixture
sources. Assertions specify exact binary installation, retained original/newer
bytes, creation/deletion Undo, closed managed metadata, stale index/file guards,
generic-API bypass refusal and exclusive-create refusal. These assertions have
not run. Actual SDK loader/all Draft 2020-12 schemas pass for 207 operations,
including three working-file contracts, bounded data/effects, permissions/groups,
noncancellable ownership, typed adapter, three ABI2 exports and EN/FR controls.
Runtime filesystem/Git/SDK/plugin/GUI/themes/DPI/POSIX acceptance remains pending
during the Kaspersky suspension; see [Git working files](GitWorkingFiles.md).

Structural Git conflict work extends the existing opt-in VCS target with a source
fixture, without another executable target. Ten distinct C++ model/service/UI/
integration/fixture sources pass MSVC /Zs; no test objects/DLLs/EXEs are built or
launched. Assertions specify complete/malformed NUL records, SHA-1/SHA-256/modes,
path ambiguity, absent sides, stale previews, binary/modify-delete/rename-rename
groups, unrelated staged data and dirty-buffer/file preservation. These native
assertions have not run. Actual CyToolsCore load_manifest and all Draft 2020-12
schemas pass for 204 operations, including four conflict contracts, effects,
permissions/groups, noncancellable ownership, typed adapter and EN/FR controls.
Runtime Git/hooks/transport/index/filesystem/concurrency, SDK/plugin/GUI and POSIX
acceptance remain pending; see [Git conflicts](GitConflicts.md).

Three Git milestones extend the existing workspace-session and VCS fixture sources,
without another test target. MSVC /Zs checks fourteen distinct model/service/UI/
plugin/test sources. Assertions specify optional legacy-compatible Git session
selection, automatic/root/identity/path guards, maintenance request boundaries,
read-only preview, stale token/parent identities, occupied/open destinations,
staged/untracked/ignored retention, manual relocation, orphan/external/sibling
refusal and actual index/link outcomes. Final native bounds/service/UI checks
pass separately. No native assertion, test object/DLL/EXE or Git fixture was run/built.
Data-only examples use the actual SDK loader and all Draft 2020-12 schemas for
200 operations, including the three new typed contracts, observed guards/effects,
bounds, permissions/groups, noncancellable ownership, pure adapter, eight ABI2
exports and EN/FR controls. Runtime/real Git/session/GUI/themes/DPI and Linux/macOS
acceptance remain unexecuted during the Kaspersky suspension.

Submodule URL/tracking-branch settings extend the existing pure and Git fixture
sources with shared address validation, branch/default/dot argv, ordered unrelated
configuration/duplicate records, staged/unstaged retention, stale parent branch/
preview, normal flags, open metadata, local overrides, redacted unsupported values
and real URL/branch/default postconditions. Ten sources pass MSVC /Zs, with final
service/UI/fixture checks repeated after changes. These assertions were not run.
Data-only checks through the actual SDK loader accept all 197 operations and
validate request/output/guard/effect/bound schemas, ownership, permissions, pure
parameter adaptation, ABI2 exports and English/French controls. No SDK Runtime,
native fixture/config mutation or GUI was launched. Runtime/concurrency and
themes/keyboard/DPI/Linux/macOS acceptance remain pending during the Kaspersky
suspension; see [Git submodules](GitSubmodules.md).

Submodule deinitialization extends the existing pure/VCS fixture sources rather
than creating a test executable. MSVC /Zs checks ten model/service/UI/plugin/test
sources. Assertions specify fixed quiet/no-force argv, complete normal index
flags, conflicts/nested gitlinks, stale preview, ignored/untracked/hidden files,
open native documents, unrecorded child commits and actual retention/removal.
No fixture or native assertion was run. Data-only checks through the actual SDK
loader validate all 195 operations, preview/apply schemas, effects/guards/bounds,
permissions, ownership, real pure parameter adapter, plugin exports and EN/FR
controls. Git removal and filesystem concurrency, SDK Runtime, GUI/themes/DPI and
Linux/macOS acceptance remain pending during the Kaspersky suspension.

Submodule addition extends the existing pure and Git repository fixture sources.
Assertions specify normalized fixed argv, address/name/branch bounds, read-only
previews, stale preview/parent branch/directory rejection, tracked-but-absent and
orphan metadata destinations, config-name reuse, hidden .gitmodules edits and
open-document guards. They are MSVC /Zs compilation checks only; no fixture was
run, no clone was attempted and no native test objects/DLL/executable were built.
Actual SDK loader/schema data checks accept 193 operations, typed local previews
with unresolved remote commit, required observed guards, real pure parameter
adaptation, permissions/cancellation and translated controls. Git/network/index/
filesystem/concurrency, SDK/plugin Runtime, GUI and platform acceptance remain
unexecuted; see [Git submodules](GitSubmodules.md).

Submodule parser/argv assertions join the existing VCS test target through
git_submodule_tests.cpp. They specify NUL/Unicode/newline records, SHA formats,
conflict stages, duplicate/path/count limits and exact single-module init/sync/
checkout arguments without force/recursive/custom modes. MSVC /Zs covers native,
UI/routing/plugin and regression sources without building or executing tests.
The real SDK loader/schema data checks accept 191 operations, typed catalogue/
process states, required observed guards, permissions, cancellation metadata,
translated controls and the actual pure Git parameter adapter. Real Git operations,
filesystem/transport guards, external races, SDK/plugin Runtime, GUI and other OSes
remain unexecuted; see [Git submodules](GitSubmodules.md).

Run-to-cursor assertions join the existing debugger target through
run_to_cursor_tests.cpp; command-profile and captured document/context assertions
extend commands_tests/module_tests. MSVC /Zs passed sixteen distinct core/UI/test
sources and the final Debugger/CommandUI adjustments without building test objects,
DLLs or executables. Cases specify original breakpoint preservation, disabled wire
mapping, destination collisions, bounds, guards, actual verification/hit IDs,
deferred events, stop ordering and uncertain restoration. The actual SDK loader/
schema data checks accept 187 operations, mandatory source/context fields, typed
lease/repair outcomes, permissions, noncancellable metadata and translated controls.
Actual native assertions, transport races/late replies, disk/source/binary changes,
adapter cleanup, SDK/plugin Runtime, GUI and platform acceptance remain unexecuted;
see [Run to cursor](RunToCursor.md).

Set-next-statement assertions join the existing debugger target through
goto_target_tests.cpp, with captured-option/context assertions in module_tests.
MSVC /Zs covers seventeen native/UI/test sources and produces no test object,
fixture DLL or test executable. Existing step-in and execution-epoch assertions
also compile against the common DebugTargets implementation. Actual SDK loader/
schema data checks accept 185 operations and typed selected-source/goto/pending
states, bounds, permissions, noncancellable execution and translated controls.
VM moves, event/deadline behavior, SDK/plugin runtime and keyboard/graphical/
multiplatform acceptance remain unexecuted; see [Set next statement](GotoTargets.md).

Targeted step-in adds step_in_target_tests.cpp to the existing debugger target
and captured-handle/frame/options assertions to module_tests.cpp. MSVC /Zs
checks fifteen native/UI/test sources without linking or running tests; the final
Debugger/UI/target assertions were checked again after the last adjustments.
Actual SDK loader/schema examples validate 183 operations, typed catalogues and
targeted acknowledgements, explicit guards, bounds, legacy field isolation,
process-only permissions, noncancellable execution and translated controls.
Context equality, target lifecycle and event behavior are native assertions,
not an executed schema guarantee. Real adapters, SDK/plugin runtime and graphical/
multiplatform acceptance remain pending; see [Targeted step-in](StepInTargets.md).

Instruction breakpoint assertions join the existing debugger target through
instruction_breakpoint_tests.cpp. Common verification/data contracts and module
draft assertions are checked with /Zs only. No native assertions, fixtures,
test objects/DLLs/executables, SDK Runtime or GUI are executed. Actual SDK loader/
schema data checks cover 181 operations and typed instruction states, guards,
bounds, permissions, noncancellable metadata and English/French labels. Actual
adapter/concurrency/graphical/platform acceptance remains pending; see
[Instruction breakpoints](InstructionBreakpoints.md).

Advanced execution adds debug_execution_tests.cpp to the existing debugger target,
and context/option capture assertions to module_tests.cpp. They are /Zs source
checks only, with no generated test object/DLL/executable or fixture execution.
Cases cover legacy wire defaults, capabilities, guard/frame mismatches, restart
hints, options, bounds, actual thread reports and immediate-stop ordering. Actual
SDK data/schema checks cover all 180 operations and the new contracts. Backend
record/reverse/restart, thread scheduling, SDK/plugin runtime, GUI and Linux/macOS
acceptance remain pending; see [Execution controls](DebugExecution.md).

Value/expression/memory mutation contracts add debug_mutation_tests.cpp to the
existing debugger target. MSVC /Zs checks its assertions without objects or native
execution: capability/context/frame bounds, readOnly preference, exact wire bytes,
base64 padding, missing/partial counts, response and hex limits. Actual CyToolsCore
loader/schema examples validate 177 operations, typed lastMutation states,
permissions/noncancellable handlers and English/French labels. Target execution,
adapter errors/late events, memory protection/concurrency and GUI acceptance remain
suspended; see [Value and memory editing](DebugValueEditing.md).

JDWP endpoint/payload/PID routing/persistence assertions extend existing debugger
sources, and typed Java attach draft assertions extend module_tests.cpp. The
Unreal extension's native catalogue expectation includes both built-in adapters.
These receive /Zs syntax checks only, without new test objects/DLLs/executables,
network connections, DNS lookup, SDK Runtime, fixtures or GUI execution. Real SDK
loader/schema examples cover bounds, legacy/default profiles and isolation of the
PID-only schema branch. See [Java debugging](JavaDebugging.md).

Java launch option/capability/wire-argument assertions extend the existing
debug_inspection_tests.cpp and adapter catalogue/revocation assertions extend
debug_tests.cpp. No additional test executable was introduced. Current checks
are MSVC /Zs only: no assertions, JVM/JDT LS, debugger socket or GUI run. Real
CyToolsCore loader/schema examples validate all 170 operations and Java profile/
project outputs without Runtime/worker execution. See [Java debugging](JavaDebugging.md).

Annotation validation/confirmation assertions extend the existing core,
code-action, extension and workflow-UI targets (annotation_tests.cpp and existing sources),
with no new executable. They cover effective-edit grouping, byte/count bounds,
annotated versioned actions, cross-plan/missing acknowledgements, apply/undo and
GUI approval resets and host export/permission/read-callback guards. Current
checks are MSVC /Zs only; no fixture server, native
assertion or GUI is run. Real SDK schema examples validate the updated additive
contracts without Runtime/worker execution. See [Annotated changes](ChangeAnnotations.md).

Session editor/image metadata and view replay assertions extend the existing
core, workflow-UI and image targets, without an additional runner. They cover
legacy records, field/collection/finite-number guards, pending settings, stale
fingerprints, Undo preservation, explicit commands and resized image framing.
Only MSVC /Zs is currently run; themes/densities/DPI fixtures, actual graphical
restoration and native lease/disk behavior remain unexecuted. See
[Workspace sessions](WorkspaceSessions.md).

Semantic-token reconstruction and cache assertions extend the existing cycode_tests
target (semantic_token_tests.cpp), with no additional runner. They cover unordered
integer edits, immutable bases, Unicode conversion, malformed/oversized responses
and LRU limits. Current verification is MSVC /Zs only; actual delta negotiation,
RPC/full fallback, close/restart/refresh concurrency and GUI behavior remain
unexecuted. See [Semantic updates](SemanticTokenUpdates.md).

CMake action-preset metadata assertions extend the existing cycode_build_tests
target (preset_action_tests.cpp), with no new runner. They cover workflow ordering
and configure links, inherited environment/output paths, hidden/escaped presets,
stale plans and new user-preset files. Current verification is MSVC /Zs only;
metadata fixtures and actual CTest/CPack/workflow execution remain suspended.
See [Preset actions](CMakePresetActions.md).

Hierarchy range/item/routing and Rider/Visual Studio/VS Code keymap assertions
extend the existing cycode_tests target (hierarchy_tests.cpp), with no new runner.
Their current validation is MSVC /Zs only. Live LSP preparation/expansion, node
sessions/expiry/concurrency, plugins/MCP and all GUI interactions remain
unexecuted; see [Hierarchies](Hierarchies.md).

Nested Git/worktree assertions extend the existing cycode_vcs_tests target
(git_repository_tests.cpp), with no additional runner. They cover NUL catalogues,
workspace/repository path translation, unrelated buffers, hunk isolation, stale
catalogues, locks, open documents, untracked preservation and external boundaries.
Only MSVC /Zs syntax checks are currently run: no test object, test executable,
Git fixture or native service is created/executed. Actual GUI/multi-instance and
Git behavior remains pending; see [Git worktrees](GitWorktrees.md).

Untitled-document contracts are added to the existing opt-in `cycode_tests`
target (draft_document_tests.cpp); no new executable is introduced. Current
verification uses MSVC /Zs only for these sources, plus production builds with
tests off. Draft recovery, GUI cancellation and editor Undo acceptance have not
been run during the Kaspersky interruption. See [Untitled documents](UntitledDocuments.md).

Ordinary builds compile CyCode, its CLI and bundled plugins with `BUILD_TESTING=OFF`.
They do not compile native test executables or run CTest. A fresh direct CMake
configuration also defaults to tests off. Existing CMake caches retain explicit
user choices; pass `-DBUILD_TESTING=OFF` when reusing an old cache directly.
The build script always sets this option explicitly.

For routine source verification:

```sh
python app/scripts/verify_sources.py
python app/scripts/build.py --plan --no-install
```

The first command parses Python syntax and JSON source data without importing
project modules, generating executables, launching native tools or downloading
dependencies. It does not check C++ compilation or application behavior. The second
prints the planned commands without running them or fetching dependencies.

To compile the application without copying over the root executable:

```sh
python app/scripts/build.py --no-install
```

The usual source/dependency/tool setup still applies; this is a real C++ build.
It does not open the application. `--core-only` limits it to the core/CLI/plugins.

## Explicit native validation

The following commands deliberately create native test executables and fixture
libraries. Some suites compile and execute generated C/C++/.NET/Rust/Java projects,
load plugin DLLs, start language servers, spawn child processes or debug programs.
They must not run as a side effect of compiling or checking source files.

```sh
# Compile native tests, without executing them or replacing the root app.
python app/scripts/build.py --build-tests
# Compile and execute the suite, serially; stop at the first failure.
python app/scripts/build.py --run-tests
# Core/CLI version, without CyUI.
python app/scripts/build.py --core-only --run-tests
# Filter execution; compilation still builds the native suite.
python app/scripts/build.py --run-tests --test-filter "^(commands|completion_defaults)$"
```

Normal build caches use `runtime/build[-vs]` or `runtime/core-build[-vs]`.
Test caches use `runtime/tests-build[-vs]` or `runtime/tests-core-build[-vs]`.
Test builds never copy an executable to the project root. The runner sets
`TEMP`, `TMP` and `TMPDIR` to `runtime/test-workspaces/native` or `core`, so suites
that use the system temporary-directory API keep owned fixtures in an identifiable
project directory. Suites with explicit `data/validation` fixtures retain them.
There is no antivirus exclusion for these directories.

The PowerShell wrapper exposes `-BuildTests`, `-RunTests`, `-TestFilter`, `-NoInstall`
and `-Plan` with the same behavior. CI opts into core native validation explicitly;
the configured matrix has not been run on GitHub locally.

The MCP `unittest` suite and `verify_*.py` desktop/build/debugger scripts are
separate integration checks. They can also launch native executables or compile
fixtures. They are not part of the source-only check or ordinary build. Choose
them explicitly when the relevant environment is ready. Desktop acceptance must
use hidden windows and preserve any instance the user already has open.

## Antivirus interruption

Kaspersky has been reported deleting native test executables on this workstation.
The detection name/report has not been inspected, so no false-positive diagnosis
is established. Local native validation is currently suspended. Source checks
remain available; do not treat them as a substitute for pending runtime validation.

On deletion or launch failure, stop the run and keep the build/test log. Do not
loop on recompilation, restore quarantined executables, disable protection or add
folder-wide exclusions. Inspect the antivirus report for the exact file and
detection first. The build runner does not retry a failed test run. Its separate
build and execution options permit reviewing generated files before any launch.
An unsigned executable can still be detected when explicitly built or run; this
workflow does not promise to change the antivirus verdict.

See [Validation](Validation.md) for results and pending failures.

Plugin-package contracts join the existing `cycode_extension_tests` target rather
than creating another executable. They specify real Ed25519 fixture signatures,
untrusted/stale selection refusal, immutable updates/rollback, shared version
leases, inactive cleanup, corrupted payload/metadata, dependency and path guards.
They use an existing compiled fixture DLL and a deterministic public test-only
seed; they are still native tests and remain unexecuted during this interruption.
Routine validation can check the signing helper's lexical paths/duplicate JSON
keys in-process without creating keys, DLLs, packages or workers. Existing automated
GUI sessions skip the real installation manager, so those fixtures cannot mutate
the user's store. New graphical package acceptance needs its own owned installation.

Installation regression sources specify allocation-unit rounding/budgets, stable
failure codes, exact/changed/linked/busy pending markers, preparation failures and
verified ordinary rollback. Compile-time assertions evaluate the actual policy
without executing fixture bodies. Ordinary builds do not enable or run these
native tests; actual disk-full/quota/recovery acceptance remains pending. See
[Installation checks](UpdateInstallation.md).

Recovery preparation adds sixteen assertions on the actual bounded copy/retention
policy. Opt-in native source fixtures specify observed allocation, ignored supplied
capacity fields and all-copy staging before a busy first target, with intact
installation/marker and retained receipts. Those bodies remain unexecuted; /Zs
and pure data authoring checks do not establish real disk-full/lock/copy/helper
acceptance. See [Recovery preparation](UpdateRecovery.md).

Recovery resumption adds twelve assertions on the actual headroom policy and
source fixtures for multi-attempt/mixed-directory reuse, changed-copy retention,
advisory phase handling, foreign/missing identities, unsafe maps/hardlinks and
mandatory backups. Native bodies remain unexecuted during the antivirus
interruption; source/build/data checks cannot establish real recovery acceptance.
See [Recovery resumption](UpdateRecovery.md).


## Explicit remote platform validation

The native GitHub matrix targets Windows Server 2022 / Visual Studio 2022,
Ubuntu and macOS. The CI invocation explicitly opts into --run-tests and
--keep-going-tests so one runner collects ordinary independent failures in one
pass. Default local behavior still stops at the first test failure. Collection
does not rebuild/retry failures or change antivirus settings, and ordinary
application builds remain BUILD_TESTING=OFF. Data-only build-plan checks cover
these boundaries. Three compiler platforms are not equivalent GUI platforms.

Production desktop CI separately compiles the GUI/CLI/providers on all three
platforms. Its Linux job explicitly runs verify_graphical_ci.py inside Xvfb/Mesa
against a synthetic workspace. It captures actual hidden Graphite/English and
light-grey/French rendering, checks active source, theme, clean buffers and source
preservation, and retains bounded reports/images as workflow artifacts. That
script refuses local invocation. Windows/macOS jobs compile only; no GUI/input,
DPI, native updater or clean-machine installer acceptance is inferred from them.

# Unreal Editor output inspection

The **Unreal build outputs** section of Unreal Engine inspects build metadata and
file presence without executing UBT, starting an editor or loading a binary.
Choose the bound launch profile or current Unreal settings, then inspect. The
last result shows its actual target/platform/configuration; changing the controls
does not turn that observation into a fresh check. File lists are virtualized and
scroll horizontally. Copying the report is explicit.

`unreal_artifacts` / native `unreal.artifacts` expose the same read-only service to
SDK/ABI2. Bundled Unreal provider 1.5.0 advertises `artifacts`; ABI remains 2. No
permission or workspace-mutation group is required for this metadata query. Its
64 MiB SDK memory reservation does not cap the whole IDE or external tool memory.

## Assessment scope

The supported ordinary layout is a modular UE4/UE5 desktop Editor target using
Development or DebugGame. Selected engine/project directories come from the
existing native inspector. The artifact pass then checks:

- A nonempty selected editor executable and shared engine `.modules` BuildId.
  DebugGame uses the engine's shared Development module manifest and the
  configuration-specific editor executable.
- A project `.target` receipt for projects declaring modules or having
  Source-owned module rules. TargetType must be Editor, and target name, platform,
  configuration, project reference, BuildId and optional launch path must match.
  The target filename alone is not proof of target type.
- The project's root module manifest, plus project-owned `.modules` files listed
  by the receipt. This includes receipt-listed project plugin manifests. Their
  BuildIds must match the selected engine; their module filenames must be plain
  filenames and refer to nonempty regular files.
- Nonempty project-owned receipt products of type DynamicLibrary or Executable.
  Engine build-product inventories, symbols and other resources are excluded.

Content-only projects do not require a project code receipt or root module
manifest. This assessment does not resolve project/plugin activation, target or
module descriptor filters, external/unlisted plugin directories, transitive
dependencies or the complete installed engine. In particular, an unlisted
content-only project's code plugin is outside this assessment. Receipt-listed
project plugin outputs are checked without guessing plugin enablement.

The service does not load or validate PE/ELF/Mach-O, architecture, ABI, PDB/symbol
quality, package installation or source freshness. Matching IDs and file presence
do not prove successful module loading or compilation against the latest closed
source files. Engine/module IDs are never rewritten to make a mismatch disappear.

Custom monolithic editors, architecture-suffixed receipts/manifests and other
nonstandard layouts need an adapter; missing/unknown evidence remains unverified.
The existing explicit detachment/manual profile workflow remains available and
leaves final module acceptance to Unreal. No new parallel engine implementation
or copied Epic code is introduced.

## Bounds and results

The artifact pass reads at most 4 MiB per metadata file, 16 MiB in aggregate, 256
metadata files and 8192 receipt products, and records up to 4096 entries and 128
issues. Its cooperative five-second budget is checked between filesystem work;
it cannot interrupt an OS filesystem call. Initial workspace/engine discovery
continues to use the existing inspector and its separate bounds. Large metadata,
unknown path variables, escaping canonical paths, malformed data, metadata
changes during reading and exhausted budgets produce an incomplete assessment.
Only `$(EngineDir)` (the installation's Engine directory) and `$(ProjectDir)`
receipt roots are expanded; no shell or environment expansion is involved.

Responses contain `supported`, `complete`, `ready`, `receiptRequired`, profile,
receipt/engine-manifest summaries, product/manifest/module lists, issues and an
observation `revision`. Ready means supported + complete + no detected issue in
this scope. Missing/incompatible files can be a complete negative assessment;
parse/limit/race failures are incomplete. The revision fingerprints the report,
not file contents, and is not an issued execution permit or frozen snapshot.

## Build and launch behavior

Native `unreal.build` retains its actual process exit/output/cancel/timeout data
and adds an artifact assessment after inspection. A failed postflight/profile
save is reported through `inspectionError`, retaining process data and marking
the project unavailable when necessary. Preparation/index consumers handle that
uncertainty instead of treating an unavailable project as a configured model.
A zero process exit remains distinct from artifact readiness.

Desktop compilation-before-debug requires a complete, ready artifact assessment
for the matching bound profile before continuing. Native bound `debug.run` and
`debug.launch` recheck current artifact metadata/files before starting a terminal
or DAP launch; SDK and plugin callers use the same owner. Configure remains
possible before the first project compilation. Attach-by-PID is unchanged.
Existing profile/draft/build/document guards, process ownership and cancellation
remain in force. This does not make build/check/launch one atomic transaction:
files can change after inspection, and Unreal remains the runtime authority.

Windows production compilation, MSVC /Zs and actual SDK schema validation are
performed locally. Native fixture assertions, actual Unreal build/module loading,
filesystem races, GUI/keyboard/themes/DPI and Linux/macOS execution remain
unverified during the current Kaspersky suspension. No test/editor process is
launched for this milestone.

Metadata semantics were read from installed UE 5.3 TargetReceipt, ModuleManifest
and WriteMetadataMode sources and receipt examples, alongside Epic's primary
[BuildID documentation](https://dev.epicgames.com/documentation/unreal-engine/how-to-version-binaries-in-unreal-engine).
Epic engine sources/binaries are not copied or distributed by this addition.

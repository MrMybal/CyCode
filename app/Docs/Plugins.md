# Native extension SDK: ABI 1 and ABI 2

ABI2 exposes language.documentHighlights through the same permission-free native
read as the GUI and MCP. Contributed servers must advertise documentHighlightProvider;
status includes documentHighlights. Output retains source identity/revision, backend
epoch, query position and normalized Text/Read/Write ranges. C ABI layout/version
and edit/process permissions remain unchanged; no STL/ImGui object crosses the
boundary. The host's independent occurrence outlines preserve plugin glyph
annotations. See [Document occurrences](Occurrences.md).

ABI2 exports language.declaration, language.typeDefinition and
language.implementation through the same native owner as definition/references.
All five expose source identity/backend/method/position provenance and bounded
validated Location/LocationLink rows. The JSON C ABI and read permission model
remain; no STL/ImGui object crosses the boundary. Contributed backends use their
actual advertised capabilities. See [Source navigation](SourceNavigation.md).

ABI2 language.symbols/documentSymbols/status and the new language.symbolResolve
export share the native hierarchical catalogue, document identity/revision and backend epochs. The JSON
C ABI and read-only permission model remain. Resolve accepts only issued session/
index pairs, retains original server data natively and uses bounded expiry/cache
and workspace/monitor/backend guards; it does not execute plugin/server commands.
Contributed enabled language servers appear in the palette selector; the chosen server's actual
advertised methods determine availability. Flat responses are preserved as roots;
no host-invented parser/tree is registered. See
[Symbol navigation](SymbolNavigation.md).

ABI2 exports read-only build.location with supplied text/directory and no
additional permission. Existing source guards, parser and response are shared
with the GUI/SDK through the unchanged JSON C ABI. Reading the output itself
still requires process.execute. language.status diagnosticRevisions now carries
document id/revision/URI, available to plugin consumers checking freshness. See
[Compiler locations](CompilerLocations.md) and [Diagnostics](DiagnosticNavigation.md).

ABI2 exports project.status and project.output through the existing JSON host
service. Output requires process.execute but is readable from read-only callbacks;
status is permission-free metadata. Actual task/context/journal ownership remains
native, with no STL observer or new C ABI crossing the plugin boundary. Trusted
plugin processes are not automatically captured; see [Project output](ProjectTaskOutput.md).

ABI2 exports build.output as a read-only host service requiring process.execute
because compiler output can contain private data. Read-only provider callbacks
may call it with that permission. The existing JSON C ABI is unchanged, with no
STL or native output callback exposed to plugins. Output from arbitrary native
plugin processes is not captured; see [Live build output](LiveBuildOutput.md).

ABI2 exports read-only unreal.artifacts without additional permissions; provider
1.5.0 advertises artifacts. Native debug run/launch checks current scoped Editor
outputs, and Unreal build results include an assessment or postflight uncertainty.
Configuration remains possible before compilation. No new C ABI or process owner;
actual extension/Unreal acceptance is pending. See [Unreal outputs](UnrealArtifacts.md).

Project provider callbacks for inspect, artifacts, launchProfile and automationPlan
retain host read access and reject host mutation services, even if the plugin has
workspace.write. These restrictions cover host callbacks, not arbitrary in-process
native plugin code. Custom revision-control actions keep their existing dispatch.

Prepared launch profiles now carry a persistent unrealLaunch binding in the same
JSON host services. debug.configure/run/launch use the native Unreal validator;
null explicitly detaches for manual editing and legacy unbound profiles remain
supported. No STL/context crosses the C ABI and no extra host operation is added.
Bundled Unreal provider 1.5.0 uses the updated host behavior. Actual extension/
Unreal runtime acceptance remains pending; see [Unreal launch](UnrealLaunch.md).

ABI2 exports unreal.launchProfile through the same native inspector/profile
generator as desktop/SDK. It requires process.execute, matching debug.profile,
because its observed profile can contain a private environment; read-only provider
callbacks may prepare but cannot save/launch. debug.configure accepts optional
expectedProfile for conflict-safe saving and still requires workspace.write.
Bundled Unreal provider 1.5.0 advertises launchProfile. No ABI/dependency change;
actual plugin acceptance remains pending. See [Unreal launch](UnrealLaunch.md).

ABI2 exports unreal.automationPlan/automationRun for reviewed cooking/packaging.
Plan is read-only; run requires process.execute and workspace.write, rejects
read-only callback mutation and uses the common cancellable build owner/events.
Issued previews/guards, persistent outputs and actual/uncertain outcomes match
desktop/SDK. Bundled Unreal provider 1.5.0 advertises both actions with these
permissions. No ABI change; actual plugin acceptance remains pending. See
[Unreal automation](UnrealAutomation.md).

ABI2 exports vcs.replay.state/plan/apply through the shared reviewed cherry-pick/
revert owner. State/plan are read-only; apply requires process/workspace/VCS
permissions and rejects mutation from read-only callbacks. Typed Git selection,
guards, persistent intents and actual/uncertain outcomes match the desktop/SDK.
No ABI/CyUI module change; actual plugin acceptance remains pending. See
[Git replay](GitReplay.md).

ABI2 exports vcs.merge.state/plan/commit/quit through the reviewed native owner.
State/preview are read-only; commit/quit require all process/workspace/VCS
permissions and retain noncancellable ownership. Message/hooks policy, metadata/
index/document guards, persistent intent and actual/uncertain outcomes match the
desktop/SDK. No ABI/CyUI module change; actual plugin acceptance remains pending.
See [Merge workflow](GitMergeWorkflow.md).

ABI2 adds vcs.checkout.plan/apply/undo for the shared working-file owner. Preview
is read-only; apply/Undo require process/workspace/VCS permissions and retain
noncancellable ownership. Binary payloads stay private to Resources; generic
resource IDs cannot bypass Git preview guards. No ABI or CyUI module change;
actual plugin acceptance remains pending. See [Git working files](GitWorkingFiles.md).

ABI2 exports `vcs.conflict.list/inspect/plan/apply` through the common native Git
owner. Only apply mutates, requiring process/workspace/VCS permissions; provider
read-only callbacks cannot use it to mutate. Explicit repository/index/preview
guards, index-only effects and actual/uncertain outcomes match desktop/SDK.
No C ABI or CyUI module changes; native plugin acceptance is pending. See
[Git conflicts](GitConflicts.md).

ABI2 now exports eight shared worktree methods: catalogue/plan are read-only,
add/remove/move/repair require VCS/workspace/process permissions, and lock/unlock
retain their existing VCS permission contract. Read-only provider callbacks cannot
mutate through these exports. Move/repair use the native observed preview and
postflight owner, including complete linked-set repair audit and uncertainty.
C ABI and CyUI dependencies remain unchanged; real plugin acceptance is pending.
See [Git worktrees](GitWorktrees.md).

ABI2 `vcs.submodule.settingsPlan` and `vcs.submodule.settings` share the same
reviewed URL/tracking-branch service as the desktop/SDK. Preview has no mutation
permission; application requires all VCS/workspace/process permissions and retains
noncancellable ownership. Old unsafe values stay private, other metadata/index/
checkout state is checked, and partial outcomes remain explicit. C ABI and module
dependencies are unchanged; native/plugin runtime acceptance remains pending.

ABI2 also exports `vcs.submodule.deinitPlan` (inspection) and
`vcs.submodule.deinit` (all VCS/workspace/process mutation permissions). They share
the native reviewed cleanup guards and retained Git metadata, typed actual outcome
and noncancellable process ownership. There is no ABI, dependency or module change.
Real plugin/runtime acceptance is pending; see [Git submodules](GitSubmodules.md).

The public contract is app/include/cycode/PluginAPI.h. Core/CLI remain free of SDL,
ImGui and CyUI; the desktop renders the contributed models with its selected
CyUI modules. Native extensions are trusted platform-specific code. The host
loads installed manifests from runtime/plugins and extensions beside the
installation, plus verified managed packages from the installation store. It never
discovers code from a newly opened project's arbitrary plugin directory.

Six bundled extensions are built with CyCode: the ABI1 Text statistics reference,
the ABI2 Unreal project tools, Subversion, Perforce, Mercurial and Diversion providers. Windows development
archives include their manifests/libraries under extensions/. Native binaries must be rebuilt per OS and
architecture. Signed local packages now have a separate installation/update/
removal/rollback workflow in Extensions; see [Plugin packages](PluginPackages.md).
Marketplace/network distribution, untrusted-code isolation and hot replacement
remain separate work. Bundled extensions retain the app's own update mechanism.

## Manifest, activation and lifetime

A manifest is UTF-8 JSON; library is a local basename without its extension:

    {
      "id": "example.tools",
      "name": "Example tools",
      "version": "1.2.0",
      "abi": 2,
      "library": "example_tools",
      "hostVersion": {"minimum":"0.2.0","maximumExclusive":"1.0.0"},
      "platforms": ["windows-x64","linux-x64","macos-arm64"],
      "permissions": ["workspace.write"],
      "dependencies": [
        {"id":"example.base","minimum":"1.0.0","maximumExclusive":"2.0.0"}
      ]
    }

Versions are numeric major.minor.patch (components 0–999999), not a complete SemVer range language.
Dependencies are topologically ordered; missing/incompatible dependencies,
duplicates, cycles, malformed manifests and registration failures are reported
in the inventory. Optional dependencies use optional:true. An init failure removes
that plugin's contributions and calls its available cleanup export.

`hostVersion` and `platforms` are optional. Older manifests remain accepted.
Host version bounds use the same numeric major.minor.patch syntax; `minimum` is
inclusive and `maximumExclusive` exclusive. Empty/reversed ranges, unknown range
keys, invalid platform IDs and duplicate platforms are rejected. Platform IDs are
`windows-x64`, `windows-arm64`, `windows-x86`, `linux-x64`, `linux-arm64`,
`linux-x86`, `macos-x64` and `macos-arm64`. The architecture describes the CyCode
process, which is the architecture its native libraries must match.

Compatibility is checked before opening the DLL/shared library, including for
disabled plugins. An incompatible plugin stays unloaded and appears with
`state:incompatible`, `compatibility.compatible:false` and a precise error. Its
required dependents cannot load. `plugins.list.host` supplies version, OS,
architecture, platform and host feature names; the Extensions panel displays this
host alongside compatibility errors. Declared platform support is metadata, not
evidence of a tested Linux/macOS build. No package download or alternative binary
is selected automatically.

The Extensions panel enables/disables an installed plugin for the current project.
.cycode/plugins.json stores schemaVersion:1 and an enabled object. Configuration
writes check both expectedRevision and the previous disk bytes before atomic
replacement. Disabling a required dependency of an active plugin is rejected;
enable dependencies before their dependent plugin.

A plugin disabled at startup is discovered without loading its native library.
Enabled plugins whose required dependencies are disabled remain dormant too.
Enabling a dormant plugin initializes it. Enabling its missing dependency also
initializes dependents that are already enabled in project preferences, in
dependency order; explicitly disabled plugins stay disabled. Disabling an already loaded plugin hides
its commands, menus, panels, syntax and providers; its library and state remain
pinned until host shutdown. ABI2 activation notifications let plugins stop their
background work on disable; legacy extensions without this hook remain responsible
for their own lifecycle until shutdown.

Start the GUI with --disable-plugins for a session with no installed native
extension loading. CLI syntax is cycode_cli --disable-plugins WORKSPACE METHOD JSON.
The flag does not rewrite project activation preferences.

## ABI 2 entry points and ownership

Export:

    int cycode_plugin_init_v2(const CyCodeHostV2 *host, void **instance);
    void cycode_plugin_shutdown_v2(void *instance);

Shutdown is optional, but a plugin allocating an instance must release it. Verify
abi_version == CYCODE_PLUGIN_ABI_V2 and struct_size before reading callbacks.
Return zero on successful init. Store a host copy in each instance, rather than a
DLL-global host pointer: several workspaces can share the same loaded library.

### Activation notifications

The original ABI2 host prefix remains unchanged. A trailing `uint64_t features`
advertises optional capabilities. Read that tail only when `struct_size` is at
least `sizeof(CyCodeHostV2)` for this header; existing binaries using the original
prefix and its size check continue to work. A plugin requiring lifecycle support
checks `features & CYCODE_HOST_FEATURE_ACTIVATION_V2` after checking the size.
No STL object or allocator crosses the boundary.

Export the optional hook:

```cpp
extern "C" CYCODE_PLUGIN_EXPORT void
cycode_plugin_active_v2(void *instance, uint32_t active) noexcept;
```

With that feature, initialization registers dormant plugin state. Start background
work only after `active=1`. Startup notification follows successful initialization
with active dependencies; explicit activation follows the configuration's guarded
disk commit. A failed configuration write does not notify activation. Duplicate
enabled-state requests do not repeat notifications. `active=0` follows successful
disable, and must stop/join plugin-owned work before returning. At shutdown the
host sends `active=0` to active instances in reverse dependency order before
calling their shutdown export; already inactive instances receive no extra stop.
The host selects cleanup for the manifest's ABI only: a library exporting both
shutdown families receives the matching one, preventing a second cleanup pass.

Notifications execute serially with normal plugin callbacks. They cannot veto a
configuration change, call host services (including reads), or throw across the
ABI. An escaped exception is recorded in `lifecycle.error`; native crashes cannot
be contained. `plugins.list` includes `lifecycle.supported` (null before loading)
and the last notified `lifecycle.active` state. The bundled Unreal extension uses
this hook and retains its per-instance profile across disable/reactivate.

Document/workspace/build subscriptions use the separate `events` contribution
and optional feature bits; see [Workspace events](Events.md). The lifecycle hook
alone does not provide subscriptions or a process sandbox. Plugins without it
keep their prior ABI1/ABI2 behavior.

host.contribute(context, kind, descriptor_json, callback) registers contributions
during initialization. It returns zero on success. Host invocations are serial;
callbacks receive their instance and UTF-8 JSON and must return plugin-owned JSON
valid until the next invocation. Input/output are bounded to 4 MiB. No exceptions,
STL objects, allocators or ImGui contexts cross the C boundary.
An ABI2 result with a nonempty string error is a failed operation, propagated to
the UI/MCP job as failure; catch exceptions inside the plugin and return that JSON.

host.request(context, method, parameters_json) is available from that plugin's
callback thread. It returns host-owned JSON with ok and result, or ok:false and
error; copy it before the next request. Requests from plugin worker threads or
initialization are rejected. Exported services currently are workspace.describe,
workspace.files, templates.list/preview/create,
resources.preview/children/trash/events/apply/undo,
documents.list/read/create/replace/save and unreal.inspect/
generate/build/database/prepare. Document/resource mutations and project creation require workspace.write in the
manifest; Unreal process operations require process.execute. Document revisions,
path containment and disk-save guards remain enforced by Engine.

Additional exports are `tests.discover/run/cancel` and `language.configure`
(requiring `process.execute`), and read-only `language.status/complete/hover/
signature/definition/references/symbols/documentSymbols/highlight`.

The JSON host now also exports read-only language.codeActions/codeActionPrepare,
language.rename/format and changes.preview. changes.apply/undo require
workspace.write and are refused from provider read callbacks. Annotated plans
retain passive labels and effective file groups; applying requires the issued
per-plan confirmedAnnotations IDs for every required label. The same native
guards and Undo serve the desktop and SDK; no C ABI layout changed. See
[annotated changes](ChangeAnnotations.md).

VCS/project read callbacks (detect/inspect/status/diff/log/history/show/blame)
cannot call mutating host services. These are host-service checks, not an OS
sandbox: a trusted native DLL still has the process user's OS privileges.
MCP generic plugin callbacks/configuration require plugins.execute,
workspace.write and process.execute; generic VCS actions also need vcs.write.

## Contributions

ABI2 host services also export `vcs.submodules` and guarded
`vcs.submodule.init/sync/update` for the built-in Git provider. Mutations require
`vcs.write`, `workspace.write` and `process.execute`, with observed repository and
catalogue guards; read-only callbacks cannot mutate. They share the native
process owner and return actual status rather than emulated VCS state. See
[Git submodules](GitSubmodules.md) for pending runtime acceptance.

`vcs.submodule.addPlan` performs local inspection without mutation permission.
The separate `vcs.submodule.add` requires all three permissions and the observed
preview revision before cloning/staging. It refuses destination/metadata reuse,
pins relevant index/parent context and returns actual process/refresh results.
Neither changes the C ABI or introduces a plugin-owned Git process.

Debugger host services also export `debug.memory/disassemble/exceptionInfo` and
paged `debug.variables`, with `process.execute` and stopped-generation guards.
`debug.functionBreakpoints/exceptionBreakpoints` additionally need
`workspace.write` to persist their checked configurations. All use the existing
host-owned adapter, rather than entering plugin code for inspection. See
[Advanced debugging](AdvancedDebugging.md) for capabilities, bounds and limits.

Build host services include `build.inspect/presets/status/configure/run/cancel` and
`msbuild.inspect/build/rebuild/clean/restore/evaluate/prepare`. Read operations need
no additional manifest permission; build execution and cancellation require
`process.execute`. See [Build cancellation](BuildCancellation.md) for native
reservation lifetime and the boundary of generic plugin-job cancellation.

`project.inspect` exports the common Cargo/npm/Java/Maven/Gradle/shader task model
without executing tools. `project.configure/build/test/install/script/launch/debug/cancel`
require `process.execute`, and read-only provider callbacks cannot invoke them.
Those routes retain the same native workspace guards, deadline, process owner and
cancellation as desktop/MCP requests; plugins do not supply a second task runner.
Host callbacks are synchronous and serialized. A long plugin-issued task can be
cancelled through the separate desktop/MCP `project.cancel` route; cancelling a
generic plugin job does not automatically identify and cancel its nested task.

All ABI2 contribution IDs begin with the plugin ID plus a dot; duplicate IDs and
built-in command replacement are rejected. Git's provider ID is reserved.

| Kind | Descriptor and callback |
| --- | --- |
| command | id, title, optional category, context global/document/editor, shortcuts array and when. Callback receives context, active text/revision from GUI or explicit MCP input. |
| menu | id, title, command belonging to the same plugin, location; no callback. Locations: file/edit/view/navigate/build/run/vcs/extensions/editor/project. |
| panel | id, title, optional when; callback returns the model below. Host owns docking, native detached windows, DPI, keyboard and rendering. |
| project | id, title, capabilities; callback receives action and parameters. Current generic invocation is plugins.project. |
| vcs | id, title, capabilities and optional actions; callback receives action and parameters with workspace/root. |
| syntax | id, name, extensions/filenames, optional base/wordLists/rules; no callback. Registration uses the common CyUI Code registry. |
| template | id/title/language/buildSystem/requirements, entry/openFile, bounded files and optional typed fields; no callback. The host previews and creates files through the shared project service. See [Project templates](ProjectTemplates.md). |
| languageServer | id/title/arguments/settings/languages; no callback or when. Requires process.execute; host-owned LSP process and shared semantic services. See [Development workflows](DevelopmentWorkflows.md). |
| debugAdapter | id/title/type/arguments/requests, optional launch/attach/environment/pidField; no callback or when. Requires process.execute; shared host-owned DAP worker, explicit profile selection and revocation. See [Plugin debuggers](PluginDebuggers.md). |
| events | id, optional types filters and maxBatch 1–32; automatic serial callback for active plugins, initial read-only snapshot handshake, bounded journal and error suspension. See [Workspace events](Events.md). |

when is an equality/membership object. The current GUI context contains document,
readOnly, path, provider and project (unreal/msbuild/cargo/npm/java/folder as detected).
Array values mean membership. This is intentionally not an arbitrary expression
language. Commands, palette items and menu items share availability checks.
Shortcuts use existing profile syntax, including chords; default plugin shortcuts
yield to occupied host bindings. User overrides remain stored if a plugin is absent.

### Panels and editing

A panel is an asynchronous, dockable native application panel. Opening/refreshing
calls its callback with event:read and context. The model accepts revision,
text, properties, rows and actions:

    {
      "revision": 1,
      "text": "Example model",
      "properties": [
        {"id":"enabled","title":"Enabled","value":true,"readOnly":false}
      ],
      "rows": [{"name":"Source","value":"workspace"}],
      "actions": [{"command":"example.tools.inspect","title":"Inspect"}]
    }

Property types are boolean, signed integer, floating point and text. IDs must be
stable. CyUI Inspector retains drafts; cancel does not call the plugin. A committed
edit sends event:commit, property, value, expectedRevision and context. The plugin
validates it, commits one application-owned transaction and returns the next model.
The host does not invent history for arbitrary plugin data. A plugin requiring
Undo/Redo must own that history and expose corresponding commands. Document edits
still enter the normal editor synchronization/Undo path.

Models are validated before drawing, limited to 128 properties, 2,000 rows and
64 actions. Returned error text is displayed. A panel action can invoke only its
owner's commands. The current model API is not an arbitrary ImGui/GPU callback or
a specialized shader/web editor API.

### Syntax packs

A syntax descriptor can inherit any built-in catalogue ID through base and
replace selected Lexilla wordLists, or define rules:

    {
      "id":"example.tools.config",
      "name":"Example configuration",
      "extensions":["examplecfg"],
      "rules":{
        "keywords":["option","section"],
        "types":["vector","number"],
        "lineComment":"#",
        "blockCommentStart":"/*",
        "blockCommentEnd":"*/",
        "caseSensitive":true
      }
    }

Rules also accept functions, quotes and a one-byte escape character. Names and
lists are bounded before registration; CyUI performs its own definition checks.
The host retains registration ownership across application contexts and restores
built-in associations when the last owner releases a definition. Syntax changes
never modify document text or Undo history.
The CyUI registry is process-wide: simultaneous in-process application contexts
share associations. Independent CyCode processes have independent registries.

This reuses CyUI's 101 built-in modes, including HLSL/GLSL/logs. It does not add
completion, diagnostics, LSP or debugging for a newly colored language. Custom
native lexer callbacks and debugger registration are not exported through ABI2.
The separate `languageServer` contribution now supplies analysis for additional
file types; a syntax descriptor alone still does not start a server.

## CMake preset actions

CMake plugins can also use build.presetPlan and build.presetRun through the
existing JSON host request (process.execute for running). They share native
review guards and cancellation ownership with the desktop/SDK; no C ABI layout
change is required. See [Preset actions](CMakePresetActions.md).

## Call and type hierarchies

The existing JSON host request also exports language.hierarchyPrepare,
language.hierarchyExpand and language.hierarchyStatus as read-only language
services. Plugins use issued session/node IDs, not a raw URI/server item.
No ABI layout change or separate language-process owner is introduced.
See [Hierarchies](Hierarchies.md) for bounds, backend and validation limits.

## Revision control by project/root

Git remains the built-in default. .cycode/vcs.json stores schemaVersion:1 and
mappings:[{root,provider}]. The deepest matching root selects a provider for
contextPath; the same mapping drives GUI actions, menus and native/MCP operations.
vcs.context describes the current provider/capabilities/actions/mapping revision.
vcs.configure atomically updates a mapping with expectedRevision. An explicit
provider parameter overrides selection for an individual request.

Unavailable configured providers remain unavailable, with an explicit error.
They never silently route writes to Git. Nested non-Git mappings remain supported.
Git additionally follows the active file's nearest workspace repository or an
explicit session-backed selector, with repository/identity parameters on typed SDK
operations. Linked-worktree controls now share eight ABI2 host exports with the
native Engine and SDK. See [Git worktrees](GitWorktrees.md).

Provider status returns provider, repository, branch, files:[{status,path}] and
message. Paths are checked against the workspace. Ordinary process actions return
exitCode/output/timedOut/truncated. Additional provider actions are described by:

    {
      "id":"checkout","title":"Check out file","scope":"file",
      "fields":[{"id":"description","title":"Description","required":true}]
    }

Actions must be advertised capabilities. scope is file or repository; fields are
bounded text inputs with optional default/required. Host menus and the VCS panel
adapt to these actions; Git-specific branch/stash/commit UI is hidden for other
providers. vcs.providerAction accepts action, contextPath and input for generic
MCP invocation. Unsupported capabilities fail explicitly. Mutations refuse dirty
buffers and reconcile clean buffers after the operation.

The bundled `cycode.svn.provider` implements local SVN status/diffs, history/blame,
file add/revert and mapped-copy commit/update with a separately installed client.
It uses these same descriptors, root mappings and shared review views. See
[Subversion](Subversion.md) for exact scope, guards and unexecuted acceptance.
The native test fixture separately exercises permissions and root persistence;
it is not evidence of real-server validation. The bundled
`cycode.perforce.provider` adds checked-client status/diff/history/annotations,
file checkout/add/reopen/revert/submit/locks and clean-project sync. See
[Perforce](Perforce.md) for precise mapping/changelist/stream limits and pending
real-server acceptance. The bundled `cycode.mercurial.provider` supplies JSON
status/history, Diff/annotations, exact-file add/forget/revert, commit, verified
clean update, bookmarks/named branches and configured-alias pull/push. It uses the
same mapping/UI/guard contracts; see [Mercurial](Mercurial.md) for restrictions and
pending real-client/GUI acceptance.
The bundled `cycode.diversion.provider` adds status/review, commit/reset, branch
creation, clean checkout/update and file locks using its automatic tracking/sync
model. See [Diversion](Diversion.md) for text-format limits and unexecuted runtime
acceptance. [Custom providers](CustomVersionControl.md) describes how a future
independent revision-control backend contributes its own actions without Git emulation.

Typed SDK Git operations now explicitly select the built-in workspace-root Git
service and accept only `provider:"git"`. Native GUI/CLI mappings remain provider
aware. External SDK provider calls use `vcs_provider_action` with its existing
plugin/process/workspace/VCS permissions and generic result schema; they do not
run under Git read-only permissions or Git-specific result validation.

## Unreal extension and ABI1

app/plugins/unreal is a working ABI2 example. It contributes conditional Build
commands, a dockable project overview with revision-checked profile properties,
and a project provider for detect/inspect/generate/prepare/database/build.
It reuses Engine's existing Unreal services, canonical paths and process guards.
Inspection is read-only; build/generation actions execute real trusted UBT rules.
This adds an extension seam without removing existing Unreal functionality.
The original backend/panel, Blueprint tools, Live Coding and Unreal Editor
integration have not all been extracted or implemented.

ABI1 remains supported with its original CyCodeHostV1, add_command,
add_vcs_provider and cycode_plugin_init entry point. CYCODE_PLUGIN_ABI stays 1;
new code explicitly selects CYCODE_PLUGIN_ABI_V2. ABI1 shutdown is
cycode_plugin_shutdown(void). Existing plugin IDs/callbacks keep working.

## Verification and remaining scope

CTest extensions loads real shared libraries and checks instance isolation,
dependency ordering, activation persistence, callback permissions, document/model
revisions, VCS routing and conflicting configuration writers. extension_ui renders
the actual application views in EN/FR, six themes and three DPI scales with no
native windows, and checks command/panel/syntax activation. unreal_extension uses
a real temporary .uproject and target/module files without building user sources.

Compatibility boundary, activation notification, dormant-dependency and matching
shutdown-family assertions have been added to the existing extension suite.
During the current antivirus interruption they passed MSVC syntax checks only;
they have not been executed. The app/core/CLI and bundled plugin compile on
Windows with native tests disabled. See [Validation](Validation.md).

ABI2 `languageServer` contributions now register additional file selectors and
separate host-owned LSP processes through the shared completion, diagnostics,
navigation and guarded edit paths. Registration requires `process.execute`, has no
callback and does not start the server. Disabling removes routes, stops the owner
and invalidates pending completion epochs. Language preferences and guarded
save/forget are shared native host services; inspection does not activate saved
servers and absent-plugin profiles are retained. See
[Language preferences](LanguagePreferences.md). The host also exports test discovery,
run/cancellation and language inspection/query/configuration services. See
[Development workflows](DevelopmentWorkflows.md) for descriptors and limits.

Broader event subscriptions, extension installation/update/removal,
crash isolation, arbitrary editor/GPU views, native
custom debugger protocol APIs and additional third-party VCS adapters remain work
items. These APIs do not establish complete Rider/Visual Studio feature parity.

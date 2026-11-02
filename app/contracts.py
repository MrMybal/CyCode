"""CyCode operations. Generated manifest is the public CyTools v1 contract."""
from pathlib import Path
import json

def schema(properties=None, required=()):
    return {'type': 'object', 'properties': properties or {}, 'required': list(required), 'additionalProperties': False}

STRING = {'type': 'string'}
PATH = {'type': 'string', 'minLength': 1, 'description': 'Workspace document reference; not a temporary input artifact.'}
REVISION = {'type': 'integer', 'minimum': 1}
BOOL = {'type': 'boolean'}
ARRAY = {'type': 'array', 'items': {'type': 'object'}}
DOCUMENT = schema({'path': STRING, 'text': STRING, 'revision': REVISION, 'dirty': BOOL,
                   'lineEnding': STRING, 'encoding': STRING}, ('path', 'text', 'revision', 'dirty', 'lineEnding', 'encoding'))
PROCESS = schema({'exitCode': {'type': 'integer'}, 'output': STRING, 'timedOut': BOOL, 'truncated': BOOL},
                 ('exitCode', 'output', 'timedOut', 'truncated'))
VCS_RESULT = schema({**PROCESS['properties'], 'reloaded': {'type':'array','items':PATH}, 'retained': {'type':'array','items':PATH}}, PROCESS['required'])
VCS_INSPECT = schema({'repository':BOOL,'root':STRING,'branch':STRING,'head':STRING,'branches':ARRAY,'remotes':ARRAY,'stashes':ARRAY,
                      'ahead':{'type':'integer'},'behind':{'type':'integer'},'hasUpstream':BOOL},
                     ('repository','root','branch','head','branches','remotes','stashes','ahead','behind','hasUpstream'))
BUILD_FIELDS = {'source': PATH, 'buildDirectory': PATH,
                'configuration': {'type': 'string', 'enum': ['Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel']},
                'toolchain': {'type': 'string', 'description': 'ID from toolchains_detect, or automatic.'},
                'generator': {'type': 'string', 'description': 'CMake generator name, or automatic.'},
                'target': {'type': 'string', 'description': 'Configured CMake target name; empty builds all default targets.'}}
BUILD_PROJECT = schema({'profile': schema(BUILD_FIELDS, tuple(BUILD_FIELDS)), 'configured': BOOL, 'generator': STRING,
                        'configurations': {'type': 'array', 'items': STRING}, 'targets': ARRAY, 'toolchains': ARRAY,
                        'compilationDatabase': schema({'path': STRING, 'origin': STRING, 'entries': {'type': 'integer', 'minimum': 0}, 'available': BOOL}, ('path', 'origin', 'entries', 'available')),
                        'message': STRING}, ('profile', 'configured', 'generator', 'configurations', 'targets', 'toolchains', 'compilationDatabase', 'message'))
BUILD_RESULT = schema({**PROCESS['properties'], 'project': {'anyOf': [BUILD_PROJECT, {'type': 'null'}]}}, (*PROCESS['required'], 'project'))
MSBUILD_FIELDS = {'entry':PATH,'configuration':{'type':'string','minLength':1,'maxLength':128},'platform':{'type':'string','minLength':1,'maxLength':128},
                  'project':{'type':'string','description':'Solution project ID or listed workspace-relative project path; empty builds the whole solution.'},
                  'tool':{'type':'string','enum':['automatic','msbuild','dotnet']}}
MSBUILD_MODEL = schema({'available':BOOL,'profile':schema({key:STRING for key in MSBUILD_FIELDS},tuple(MSBUILD_FIELDS)),
    'configurations':{'type':'array','items':STRING},'projects':ARRAY,'artifacts':ARRAY,'tools':ARRAY,'analysisWarnings':{'type':'array','items':STRING},'message':STRING,
    'compilationDatabase':schema({'available':BOOL,'path':STRING,'entries':{'type':'integer','minimum':0}},('available','path','entries'))},
    ('available','profile','configurations','projects','artifacts','tools','analysisWarnings','message','compilationDatabase'))
MSBUILD_RESULT = schema({**PROCESS['properties'],'project':MSBUILD_MODEL,'evaluation':{'type':['object','null']},'diagnostics':ARRAY},(*PROCESS['required'],'project'))
POSITION = schema({'line': {'type': 'integer', 'minimum': 0}, 'character': {'type': 'integer', 'minimum': 0}}, ('line', 'character'))
RANGE = schema({'start': POSITION, 'end': POSITION}, ('start', 'end'))
LOCATIONS = schema({'path': PATH, 'revision': REVISION, 'positionEncoding': {'const': 'utf-16', 'type': 'string'},
                    'locations': {'type': 'array', 'items': schema({'path': PATH, 'range': RANGE, 'external': BOOL, 'displayPath': STRING}, ('path', 'range'))},
                    'outsideWorkspace': {'type': 'integer', 'minimum': 0}, 'truncated': BOOL},
                   ('path', 'revision', 'positionEncoding', 'locations', 'outsideWorkspace', 'truncated'))
LANGUAGE_INPUT = {'path': PATH, 'expectedRevision': REVISION, 'position': POSITION,
                  'includeExternal': BOOL,
                  'limit': {'type': 'integer', 'minimum': 1, 'maximum': 5000}}
LANGUAGE_SERVER = schema({'ready':BOOL, 'running':BOOL, 'error':STRING, 'diagnostics':{'type':'object'}, 'name':STRING, 'enabled':BOOL,
                         'semanticTokens':BOOL,'semanticGeneration':{'type':'integer','minimum':0}},
                         ('ready','running','error','diagnostics','name','enabled'))
LANGUAGE_STATUS = schema({**LANGUAGE_SERVER['properties'], 'servers':schema({'cpp':LANGUAGE_SERVER,'csharp':LANGUAGE_SERVER},('cpp','csharp'))},
                        (*LANGUAGE_SERVER['required'],'servers'))
SYMBOL = schema({'path': PATH, 'range': RANGE, 'external': BOOL, 'displayPath': STRING,
                 'name': STRING, 'kind': {'type': 'integer'}, 'container': STRING, 'detail': STRING},
                ('path', 'range', 'name', 'kind', 'container', 'detail'))
SYMBOLS = schema({'symbols': {'type': 'array', 'items': SYMBOL}, 'outsideWorkspace': {'type': 'integer', 'minimum': 0},
                  'truncated': BOOL, 'positionEncoding': {'type': 'string', 'const': 'utf-16'}, 'path': PATH, 'revision': REVISION},
                 ('symbols', 'outsideWorkspace', 'truncated', 'positionEncoding'))
CHANGE = schema({'id': STRING, 'title': STRING, 'state': {'type': 'string', 'enum': ['preview', 'applied', 'undone']},
                 'files': {'type': 'array', 'items': schema({'path': PATH, 'expectedRevision': REVISION, 'editCount': {'type': 'integer', 'minimum': 0}}, ('path', 'expectedRevision', 'editCount'))},
                 'editCount': {'type': 'integer', 'minimum': 0}}, ('id', 'title', 'state', 'files', 'editCount'))
SOURCE_DOCUMENT = schema({**DOCUMENT['properties'], 'readOnly': {'type': 'boolean', 'const': True}, 'displayPath': STRING},
                         (*DOCUMENT['required'], 'readOnly', 'displayPath'))
UNREAL_FIELDS = {'project': PATH, 'engineRoot': STRING, 'target': STRING,
                 'platform': {'type': 'string', 'enum': ['Win64', 'Linux', 'LinuxArm64', 'Mac', 'Android', 'IOS']},
                 'configuration': {'type': 'string', 'enum': ['Debug', 'DebugGame', 'Development', 'Shipping', 'Test']}}
UNREAL_MODEL = schema({'available': BOOL, 'profile': schema(UNREAL_FIELDS, tuple(UNREAL_FIELDS)),
    'name': STRING, 'engineAssociation': STRING, 'engineVersion': {'type': 'object'},
    'buildTool': {'type': 'array', 'items': STRING}, 'ready': BOOL, 'targets': ARRAY, 'modules': ARRAY, 'plugins': ARRAY,
    'truncated': BOOL, 'declaredModules': ARRAY, 'declaredPlugins': ARRAY,
    'compilationDatabase': schema({'available': BOOL, 'path': STRING}, ('available', 'path'))}, ('available',))
UNREAL_RESULT = schema({**PROCESS['properties'], 'project': UNREAL_MODEL, 'cached': BOOL}, (*PROCESS['required'], 'project'))
ENVIRONMENT = {'type':'object','maxProperties':128,'additionalProperties':{'type':['string','null']}}
DEBUG_FIELDS = {'name':STRING,'program':{'type':'string','maxLength':4096},'cwd':PATH,'args':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':8192}},
                'env':ENVIRONMENT,'adapter':STRING,'adapterArgs':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':8192}},'runtime':{'type':'string','enum':['automatic','native','dotnet']},'dotnet':STRING,'stopOnEntry':BOOL,'terminal':{'type':'string','enum':['integrated','console','external']},
                'watches':{'type':'array','maxItems':32,'items':{'type':'string','maxLength':8192}},'buildBeforeLaunch':BOOL}
BREAKPOINT = schema({'line':{'type':'integer','minimum':1,'maximum':10000000},'enabled':BOOL,
                     'condition':{'type':'string','maxLength':4096},'hitCondition':{'type':'string','maxLength':4096},'logMessage':{'type':'string','maxLength':4096}},('line',))
BREAKPOINTS = {'type':'array','maxItems':1000,'items':BREAKPOINT}
DEBUG_STATE_FIELDS = {'state':{'type':'string','enum':['idle','starting','running','stopped','exited','failed']},
    'reason':STRING,'error':STRING,'threads':ARRAY,'frames':ARRAY,'scopes':ARRAY,'variables':ARRAY,'output':STRING,
    'generation':{'type':'integer'},'threadId':{'type':'integer'},'frameId':{'type':'integer'},'terminalId':STRING,
    'breakpoints':{'type':'object','additionalProperties':BREAKPOINTS},'verifiedBreakpoints':{'type':'object'},'capabilities':{'type':'object'},
    'lastBreakpoint':{'type':'object'},'stopGeneration':{'type':'integer'},'exitCode':{'type':'integer'},'watches':ARRAY,'attached':BOOL,'runtime':{'type':'string','enum':['native','dotnet']}}
DEBUG_STATE = schema(DEBUG_STATE_FIELDS,('state','reason','error','threads','frames','scopes','variables','output','generation','threadId','frameId','terminalId','breakpoints','verifiedBreakpoints','capabilities'))
OPERATIONS = [
    ('msbuild_inspect','msbuild.inspect','Inspect a .sln or direct MSBuild project, solution-to-project configurations and saved artifacts/index. Never evaluates project imports or builds targets.',schema(MSBUILD_FIELDS),MSBUILD_MODEL,[]),
    ('msbuild_tools','msbuild.tools','Detect Visual Studio MSBuild and installed .NET SDKs without loading project rules.',schema({'refresh':BOOL}),schema({'tools':ARRAY},('tools',)),[]),
    ('msbuild_build','msbuild.build','Build the selected trusted .sln/project with real MSBuild; solution project selection honors nesting/configuration mappings. Optional restore invokes package restore. Saves the successful local profile.',schema({**MSBUILD_FIELDS,'restore':BOOL}),MSBUILD_RESULT,['process.execute']),
    ('msbuild_rebuild','msbuild.rebuild','Rebuild the selected trusted solution/project, including its clean target. Runs project rules.',schema({**MSBUILD_FIELDS,'restore':BOOL}),MSBUILD_RESULT,['process.execute']),
    ('msbuild_clean','msbuild.clean','Run the Clean target of the selected trusted solution/project. Build rules determine which generated outputs are removed.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('msbuild_restore','msbuild.restore','Run package restore for the trusted solution/project through MSBuild and configured package sources.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('msbuild_evaluate','msbuild.evaluate','Explicitly evaluate one trusted workspace project with MSBuild 17.8+ and read real target properties/items. Loads imports/property functions and can execute project code; this is not a read-only XML parser.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('msbuild_prepare','msbuild.prepare','Explicitly evaluate up to eight enabled local VC++ projects and prepare a managed clangd compilation database from evaluated includes/defines/forced includes/language modes. Does not compile targets. AdditionalOptions are reported for review.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('vcs_inspect','vcs.inspect','Read branches, remotes, tracking divergence and stashes without changing the repository.',schema(),VCS_INSPECT,[]),
    ('vcs_init','vcs.init','Initialize a Git repository at the workspace root with an initial main branch.',schema(),PROCESS,['vcs.write']),
    ('vcs_history','vcs.history','Read structured commit history affecting this workspace.',schema({'limit':{'type':'integer','minimum':1,'maximum':500}}),schema({'commits':ARRAY,'truncated':BOOL,'message':STRING},('commits','truncated','message')),[]),
    ('vcs_show','vcs.show','Read a verified commit and its workspace diff.',schema({'revision':STRING},('revision',)),PROCESS,[]),
    ('vcs_blame','vcs.blame','Read line attribution for a workspace document.',schema({'path':PATH},('path',)),PROCESS,[]),
    ('vcs_branch_create','vcs.branch.create','Create a branch at a verified commit. Requires opening the repository root.',schema({'name':STRING,'start':STRING},('name',)),VCS_RESULT,['vcs.write']),
    ('vcs_branch_switch','vcs.branch.switch','Switch branch only with saved buffers and a clean tracked working tree. Reload changed clean buffers with fresh revisions. Git protects untracked files from overwrite.',schema({'name':STRING,'track':BOOL},('name',)),VCS_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_branch_rename','vcs.branch.rename','Rename a local branch.',schema({'name':STRING,'newName':STRING},('name','newName')),VCS_RESULT,['vcs.write']),
    ('vcs_branch_delete','vcs.branch.delete','Delete a merged local branch using Git safe deletion; force deletion is unavailable.',schema({'name':STRING},('name',)),VCS_RESULT,['vcs.write']),
    ('vcs_remote_add','vcs.remote.add','Add a named Git remote URL without contacting it.',schema({'remote':STRING,'url':STRING},('remote','url')),VCS_RESULT,['vcs.write']),
    ('vcs_remote_set','vcs.remote.set','Set a remote URL without contacting it.',schema({'remote':STRING,'url':STRING},('remote','url')),VCS_RESULT,['vcs.write']),
    ('vcs_remote_remove','vcs.remote.remove','Remove a remote and its tracking references.',schema({'remote':STRING},('remote',)),VCS_RESULT,['vcs.write']),
    ('vcs_fetch','vcs.fetch','Fetch and prune the selected remote. Uses configured Git credentials/transports; no interactive credential UI is provided.',schema({'remote':STRING}),VCS_RESULT,['vcs.write','process.execute']),
    ('vcs_pull','vcs.pull','Fetch then fast-forward the selected remote branch, refusing dirty buffers, tracked changes or divergence. Never creates an implicit merge commit.',schema({'remote':STRING,'branch':STRING}),VCS_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_push','vcs.push','Push the selected local branch without force, honoring Git hooks. Optional setUpstream establishes tracking.',schema({'remote':STRING,'branch':STRING,'setUpstream':BOOL}),VCS_RESULT,['vcs.write','process.execute']),
    ('vcs_merge','vcs.merge','Merge a verified commit into the current branch with saved buffers and a clean tracked tree. Conflicts remain on disk and can be inspected/resolved explicitly.',schema({'revision':STRING},('revision',)),VCS_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_stash_save','vcs.stash.save','Stash tracked working-tree changes, optionally including untracked files. Requires saved buffers and reloads changed clean buffers.',schema({'message':STRING,'includeUntracked':BOOL}),VCS_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_stash_apply','vcs.stash.apply','Apply a stash on a clean tracked tree; conflicts remain for explicit resolution.',schema({'index':{'type':'integer','minimum':0,'maximum':9999}}),VCS_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_stash_pop','vcs.stash.pop','Apply a stash on a clean tracked tree and remove it only after Git succeeds.',schema({'index':{'type':'integer','minimum':0,'maximum':9999}}),VCS_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_stash_drop','vcs.stash.drop','Permanently drop the explicitly selected stash entry.',schema({'index':{'type':'integer','minimum':0,'maximum':9999}},('index',)),VCS_RESULT,['vcs.write']),
    ('vcs_conflicts','vcs.conflicts','Read base, ours, theirs and the live working buffer for an unresolved text conflict at repository root. Missing versions are null. UTF-8 versions are bounded to 4 MiB.',schema({'path':PATH},('path',)),schema({'path':PATH,'revision':REVISION,'working':STRING,'versions':schema({'base':{'type':['string','null']},'ours':{'type':['string','null']},'theirs':{'type':['string','null']}},('base','ours','theirs'))},('path','revision','working','versions')),[]),
    ('vcs_resolve','vcs.resolve','Save and stage a conflict resolution using the live expectedRevision. Optional text replaces the live buffer first. Rejects external disk changes and remaining conflict markers unless explicitly allowed.',schema({'path':PATH,'expectedRevision':REVISION,'text':{'type':'string','maxLength':4194304},'allowMarkers':BOOL},('path','expectedRevision')),PROCESS,['vcs.write','workspace.write','process.execute']),
    ('documents_reload','documents.reload','Reload a document from disk with a fresh revision. Refuses unsaved buffers unless discard is explicitly true; validates expectedRevision.',schema({'path':PATH,'expectedRevision':REVISION,'discard':BOOL},('path','expectedRevision')),DOCUMENT,['workspace.write']),
    ('debug_profile','debug.profile','Read the persistent native or .NET debug/run profile. runtime selects automatic/native/dotnet; dotnet selects an installed host or automatic discovery. Adapter/process access is required because the profile can contain environment variables.',schema(),schema(DEBUG_FIELDS,tuple(DEBUG_FIELDS)),['process.execute']),
    ('debug_configure','debug.configure','Update and atomically save supported debug profile fields in .cycode/launch.json. Arguments are argv elements, not a shell command. Does not compile or start a program.',schema(DEBUG_FIELDS),schema(DEBUG_FIELDS,tuple(DEBUG_FIELDS)),['process.execute','workspace.write']),
    ('debug_status','debug.status','Read actual DAP state, verified breakpoints, threads, stack, scopes, variables, watches and bounded adapter output. Source lines/columns are one-based. Runtime identifiers expire on resume/relaunch.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_breakpoints','debug.breakpoints','Replace source breakpoints for one workspace file and persist them. Supports enabled flags, conditions, hit counts and logpoints according to adapter capabilities. expectedBreakpoints rejects concurrent changes.',
     schema({'path':PATH,'breakpoints':BREAKPOINTS,'expectedBreakpoints':BREAKPOINTS},('path','breakpoints')),DEBUG_STATE,['process.execute','workspace.write']),
    ('debug_launch','debug.launch','Launch a trusted native executable or runnable .NET assembly through DAP. Automatic runtime detection uses PE/CLI metadata and apphost companions; .NET uses NetCoreDbg and a compatible installed dotnet host. Defaults to the saved profile; supplied fields override this launch. The GUI separately provides build-before-debug. Requires matching debug symbols. Managed output uses the debug console; use debug_run for interactive stdin.',schema(DEBUG_FIELDS),DEBUG_STATE,['process.execute']),
    ('debug_attach','debug.attach','Attach to the explicitly supplied process ID through DAP. Stop/disconnect detaches attached processes instead of terminating them. OS permissions and adapter support still apply.',schema({**DEBUG_FIELDS,'pid':{'type':'integer','minimum':1}},('pid',)),DEBUG_STATE,['process.execute']),
    ('debug_run','debug.run','Run a trusted native executable or runnable .NET assembly in a native terminal without debugging, with interactive stdin. Uses the saved profile and optional overrides. The terminal and its processes persist until explicitly closed or the workspace/IDE exits.',schema(DEBUG_FIELDS),schema({'terminalId':STRING,'pid':{'type':'integer'}},('terminalId','pid')),['process.execute']),
    ('debug_continue','debug.continue','Continue the paused debuggee.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_pause','debug.pause','Pause the running debuggee and asynchronously inspect its actual threads/stack.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_next','debug.next','Step over the current source statement in the selected thread.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_step_in','debug.stepIn','Step into a function through DAP.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_step_out','debug.stepOut','Step out of the selected function through DAP.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_stop','debug.stop','Stop the debug session. Terminates launched debuggees and their owned terminals; detaches from an explicitly attached process, leaving that existing process running.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_evaluate','debug.evaluate','Evaluate an expression or debugger console command in the selected stopped frame. Expressions/console commands can have side effects in the target process.',schema({'expression':{'type':'string','maxLength':8192},'context':{'type':'string','enum':['watch','hover','repl']}},('expression',)),{'type':'object'},['process.execute']),
    ('debug_variables','debug.variables','Read a bounded page of children from a live stopped-frame variable reference. References become stale on resume.',schema({'variablesReference':{'type':'integer','minimum':1},'start':{'type':'integer','minimum':0},'count':{'type':'integer','minimum':1,'maximum':200}},('variablesReference',)),schema({'variables':ARRAY},('variables',)),['process.execute']),
    ('debug_frame','debug.frame','Select a current stack frame and refresh its scopes, locals and watches.',schema({'id':{'type':'integer','minimum':0}},('id',)),DEBUG_STATE,['process.execute']),
    ('debug_thread','debug.thread','Select a current thread and inspect its stack.',schema({'id':{'type':'integer','minimum':1}},('id',)),DEBUG_STATE,['process.execute']),
    ('debug_source','debug.source','Read an adapter-issued source reference while paused, including generated disassembly. This is a read-only view; it does not broaden editable workspace paths.',schema({'sourceReference':{'type':'integer','minimum':1}},('sourceReference',)),schema({'content':STRING,'mimeType':STRING},('content',)),['process.execute']),
    ('terminal_create', 'terminal.create', 'Start a persistent interactive shell in this IDE workspace. Uses native ConPTY/PTY, not a one-shot command. Sessions survive the Core job and must be closed explicitly; closing the IDE/workspace terminates their processes.',
     schema({'cwd': PATH, 'profile': {'type': 'string', 'enum': ['default', 'powershell', 'cmd', 'bash', 'zsh']}, 'env':ENVIRONMENT, 'columns': {'type': 'integer', 'minimum': 10, 'maximum': 400}, 'rows': {'type': 'integer', 'minimum': 2, 'maximum': 120}}),
     schema({'id': STRING, 'pid': {'type': 'integer'}, 'executable': STRING, 'cwd': STRING, 'title': STRING, 'running': BOOL, 'exitCode': {'type': 'integer'}, 'columns': {'type': 'integer'}, 'rows': {'type': 'integer'}, 'history': {'type': 'integer'}, 'generation': {'type': 'integer'}}, ('id', 'pid', 'executable', 'cwd', 'title', 'running', 'exitCode', 'columns', 'rows', 'history', 'generation')), ['process.execute']),
    ('terminal_list', 'terminal.list', 'List this IDE instance’s persistent terminal sessions, including user-created sessions. Requires process access because terminal data can include output beyond project documents.', schema(), schema({'sessions': ARRAY}, ('sessions',)), ['process.execute']),
    ('terminal_read', 'terminal.read', 'Read a bounded terminal screen/scrollback range as plain Unicode lines. Coordinates are zero-based display cells, not LSP positions. Output belongs to the live terminal, not this job’s artifact workspace.',
     schema({'id': STRING, 'firstRow': {'type': 'integer', 'minimum': 0}, 'limit': {'type': 'integer', 'minimum': 0, 'maximum': 2000}}, ('id',)),
     schema({'id': STRING, 'firstRow': {'type': 'integer'}, 'totalRows': {'type': 'integer'}, 'columns': {'type': 'integer'}, 'rows': {'type': 'integer'}, 'cursor': schema({'row': {'type': 'integer'}, 'column': {'type': 'integer'}}, ('row','column')), 'running': BOOL, 'alternate': BOOL, 'generation': {'type': 'integer'}, 'lines': {'type': 'array', 'items': STRING}}, ('id','firstRow','totalRows','columns','rows','cursor','running','alternate','generation','lines')), ['process.execute']),
    ('terminal_write', 'terminal.write', 'Write raw UTF-8/control input to an existing interactive terminal. A carriage return submits a shell line. This can execute arbitrary commands with the IDE user’s permissions.', schema({'id': STRING, 'data': {'type': 'string', 'maxLength': 65536}}, ('id','data')), schema({'id': STRING, 'ok': BOOL}, ('id','ok')), ['process.execute']),
    ('terminal_key', 'terminal.key', 'Send an interactive terminal key. Modifier bits: Shift=1, Alt=2, Ctrl=4.', schema({'id': STRING, 'key': {'type': 'string', 'enum': ['Enter','Tab','Backspace','Escape','Up','Down','Left','Right','Home','End','Insert','Delete','PageUp','PageDown','F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12']}, 'modifiers': {'type': 'integer', 'minimum': 0, 'maximum': 7}}, ('id','key')), schema({'id': STRING, 'ok': BOOL}, ('id','ok')), ['process.execute']),
    ('terminal_resize', 'terminal.resize', 'Resize a persistent PTY and its emulated screen.', schema({'id': STRING, 'columns': {'type': 'integer', 'minimum': 10, 'maximum': 400}, 'rows': {'type': 'integer', 'minimum': 2, 'maximum': 120}}, ('id','columns','rows')), schema({'id': STRING, 'ok': BOOL}, ('id','ok')), ['process.execute']),
    ('terminal_clear', 'terminal.clear', 'Clear retained scrollback without restarting the shell or changing files.', schema({'id': STRING}, ('id',)), schema({'id': STRING, 'ok': BOOL}, ('id','ok')), ['process.execute']),
    ('terminal_close', 'terminal.close', 'Terminate and remove one terminal and its process tree. Closing the panel only hides it; this operation ends the session.', schema({'id': STRING}, ('id',)), schema({'id': STRING, 'ok': BOOL}, ('id','ok')), ['process.execute']),
    ('language_symbols', 'language.symbols', 'Search actual C++ or C# project symbols, optionally filtered to types. language defaults to cpp; csharp requires an enabled C# server. Index completeness depends on background analysis.',
     schema({'query': {'type': 'string', 'maxLength': 256}, 'language':{'type':'string','enum':['cpp','csharp']}, 'typesOnly': BOOL, 'includeExternal': BOOL, 'limit': {'type': 'integer', 'minimum': 1, 'maximum': 1000}}), SYMBOLS, []),
    ('language_document_symbols', 'language.documentSymbols', 'Read C/C++ or C# document structure, including nested members, from the live buffer. Accepts a workspace path or an opened SDK reference.',
     schema({'path': PATH, 'expectedRevision': REVISION, 'includeExternal': BOOL, 'limit': {'type': 'integer', 'minimum': 1, 'maximum': 1000}}, ('path', 'expectedRevision')), SYMBOLS, []),
    ('sources_read', 'sources.read', 'Read an immutable SDK source view. Requires a sdk:// reference issued by symbol navigation. Arbitrary external paths and files outside registered Unreal/compiler SDK source roots are rejected.',
     schema({'source': {'type': 'string', 'minLength': 1, 'maxLength': 4096, 'pattern': '^sdk://[0-9]+/.+$'}}, ('source',)), SOURCE_DOCUMENT, []),
    ('language_rename', 'language.rename', 'Prepare a semantic C/C++ or C# rename preview across references. Does not edit buffers or save files. Unreal Blueprint assets and serialized reflection names are not migrated.',
     schema({'path': PATH, 'expectedRevision': REVISION, 'position': POSITION, 'newName': {'type': 'string', 'minLength': 1, 'maxLength': 256}}, ('path', 'expectedRevision', 'position', 'newName')), CHANGE, []),
    ('language_format', 'language.format', 'Prepare C++ clang-format or C# Roslyn formatting edits. Does not apply changes or save files. A range limits formatting to the selection.',
     schema({'path': PATH, 'expectedRevision': REVISION, 'range': RANGE, 'tabSize': {'type': 'integer', 'minimum': 1, 'maximum': 16}, 'insertSpaces': BOOL}, ('path', 'expectedRevision')), CHANGE, []),
    ('changes_preview', 'changes.preview', 'Read before/after text and concrete UTF-16 edits for one file in a prepared change. Preview IDs belong to the running IDE and expire; they are not filesystem paths.',
     schema({'id': STRING, 'path': PATH}, ('id', 'path')), schema({'id': STRING, 'path': PATH, 'before': STRING, 'after': STRING, 'edits': ARRAY}, ('id', 'path', 'before', 'after', 'edits')), []),
    ('changes_apply', 'changes.apply', 'Apply a prepared change to live workspace buffers only. Every affected revision and disk baseline is validated before any buffer changes. Does not save to disk.',
     schema({'id': STRING}, ('id',)), CHANGE, ['workspace.write']),
    ('changes_undo', 'changes.undo', 'Undo a previously applied refactoring across its live buffers. Refuses changed/closed buffers and external disk conflicts. Does not save to disk.',
     schema({'id': STRING}, ('id',)), CHANGE, ['workspace.write']),
    ('unreal_prepare', 'unreal.prepare', 'Prepare and cache Unreal code analysis, including UHT-generated headers; reuses a matching existing cache. Executes trusted project rules.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('unreal_inspect', 'unreal.inspect', 'Inspect an Unreal .uproject, engine, targets, modules and plugins without executing project rules.', schema(UNREAL_FIELDS), UNREAL_MODEL, []),
    ('unreal_generate', 'unreal.generate', 'Generate Unreal solution/project files using the selected engine UnrealBuildTool. Executes trusted project rules.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('unreal_database', 'unreal.database', 'Generate compile_commands.json for clangd via UnrealBuildTool. Executes trusted project rules.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('unreal_build', 'unreal.build', 'Compile a discovered Unreal target with UnrealBuildTool; does not cook or package assets.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('workspace_describe', 'workspace.describe', 'Describe the live workspace.', schema(),
     schema({'root': STRING, 'name': STRING, 'projectType': STRING, 'version': STRING, 'solution': {'type': ['object', 'null']}}, ('root', 'name', 'projectType', 'version', 'solution')), []),
    ('workspace_files', 'workspace.files', 'List workspace files, respecting ignore rules.', schema(),
     schema({'files': {'type': 'array', 'items': STRING}, 'truncated': BOOL}, ('files', 'truncated')), []),
    ('search_text', 'search.text', 'Search project text. Unsaved buffers override disk for literal searches. Regex excludes dirty buffers.',
     schema({'query': {'type': 'string', 'maxLength': 4096}, 'caseSensitive': BOOL, 'regex': BOOL, 'limit': {'type': 'integer', 'minimum': 1, 'maximum': 5000}}, ('query',)),
     schema({'matches': ARRAY, 'truncated': BOOL, 'timedOut': BOOL, 'unsavedRegexExcluded': BOOL, 'cancelled': BOOL}, ('matches', 'truncated')), []),
    ('documents_list', 'documents.list', 'List documents currently open in the IDE.', schema(), schema({'documents': ARRAY}, ('documents',)), []),
    ('documents_read', 'documents.read', 'Read the live document and its revision, including unsaved edits.', schema({'path': PATH}, ('path',)), DOCUMENT, []),
    ('documents_create', 'documents.create', 'Create an empty UTF-8 file inside the workspace. Refuses to overwrite an existing file.', schema({'path': PATH}, ('path',)), DOCUMENT, ['workspace.write']),
    ('documents_replace', 'documents.replace', 'Replace the live buffer using expectedRevision. Does not save to disk.',
     schema({'path': PATH, 'text': {'type': 'string', 'maxLength': 4194304}, 'expectedRevision': REVISION}, ('path', 'text', 'expectedRevision')), DOCUMENT, ['workspace.write']),
    ('documents_save', 'documents.save', 'Atomically save a live buffer, preserving BOM and line endings. Rejects external disk changes.',
     schema({'path': PATH, 'expectedRevision': REVISION}, ('path', 'expectedRevision')), DOCUMENT, ['workspace.write']),
    ('language_configure', 'language.configure', 'Enable, restart or disable the C# language server for this trusted workspace. Loads and evaluates MSBuild projects and imports. The optional executable is a server program or DLL; default uses the pinned managed runtime. Applies to the running IDE only.',
     schema({'enabled':BOOL,'executable':STRING}), LANGUAGE_STATUS, ['process.execute']),
    ('language_status', 'language.status', 'Inspect shared C++/clangd and C#/Roslyn servers and their published UTF-16 diagnostics. Does not start the C# server.',
     schema(), LANGUAGE_STATUS, []),
    ('language_highlight','language.highlight','Read semantic color classifications from the shared language server and current live document. Requires the expected revision; rejects concurrent document changes. Positions/lengths use UTF-16; glyphStart/glyphEnd are Unicode-codepoint indexes. supported=false means lexical fallback. Data is bounded to 50,000 tokens or 4 MiB and reports truncation. C# requires prior language_configure; inspection never enables it.',
     schema({'path':PATH,'expectedRevision':REVISION},('path','expectedRevision')),
     schema({'path':PATH,'revision':REVISION,'supported':BOOL,'generation':{'type':'integer','minimum':0},'truncated':BOOL,
             'tokens':{'type':'array','maxItems':50000,'items':schema({'line':{'type':'integer','minimum':0},'character':{'type':'integer','minimum':0},'length':{'type':'integer','minimum':1},'glyphStart':{'type':'integer','minimum':0},'glyphEnd':{'type':'integer','minimum':1},'type':STRING,'modifiers':{'type':'array','items':STRING}},('line','character','length','glyphStart','glyphEnd','type','modifiers'))}},
            ('path','revision','supported','generation','truncated','tokens')),[]),
    ('language_complete', 'language.complete', 'Request C++ or C# completion items from the shared server and live buffers. Returns standard LSP completion items; applying snippets, additional imports or resolve requests is not supported. C# project loading must first be enabled with language_configure.',
     schema({'path':PATH,'expectedRevision':REVISION,'position':POSITION},('path','expectedRevision','position')),schema({'path':PATH,'revision':REVISION,'result':{'type':['object','array','null']}},('path','revision','result')), []),
    ('language_definition', 'language.definition', 'Find C/C++ or C# definitions using live buffers and actual project references. Requires the current revision and a UTF-16 position. Outside-workspace locations are omitted by default; includeExternal permits only registered read-only SDK sources.',
     schema(dict(LANGUAGE_INPUT), ('path', 'expectedRevision', 'position')), LOCATIONS, []),
    ('language_references', 'language.references', 'Find C/C++ or C# references using shared servers and unsaved buffers. Positions/ranges are zero-based UTF-16. Requires the current revision; results reflect the available project index, which may still be building.',
     schema({**LANGUAGE_INPUT, 'includeDeclaration': BOOL}, ('path', 'expectedRevision', 'position')), LOCATIONS, []),
    ('vcs_status', 'vcs.status', 'Inspect Git repository status.', schema(), schema({'provider': STRING, 'repository': BOOL, 'branch': STRING, 'files': ARRAY, 'message': STRING}, ('provider', 'repository', 'branch', 'files', 'message')), []),
    ('vcs_diff', 'vcs.diff', 'Read working tree or staged differences.', schema({'path': PATH, 'staged': BOOL}), PROCESS, []),
    ('vcs_log', 'vcs.log', 'Read the latest 30 Git commits affecting the workspace.', schema(), PROCESS, []),
    ('vcs_stage', 'vcs.stage', 'Stage one workspace path.', schema({'path': PATH}, ('path',)), PROCESS, ['vcs.write']),
    ('vcs_unstage', 'vcs.unstage', 'Unstage one workspace path.', schema({'path': PATH}, ('path',)), PROCESS, ['vcs.write']),
    ('vcs_commit', 'vcs.commit', 'Commit the existing Git index, honoring repository hooks and signing settings.', schema({'message': {'type': 'string', 'minLength': 1, 'maxLength': 4096}}, ('message',)), PROCESS, ['vcs.write']),
    ('toolchains_detect', 'toolchains.detect', 'Detect installed CMake, Ninja, Make and C/C++ toolchains. Queries tool versions; does not configure or execute project code.', schema({'refresh': BOOL}),
     schema({'cmake': STRING, 'cmakeVersion': STRING, 'ninja': STRING, 'make': STRING, 'toolchains': ARRAY, 'generators': ARRAY, 'warnings': {'type': 'array', 'items': STRING}}, ('cmake', 'cmakeVersion', 'ninja', 'make', 'toolchains', 'generators', 'warnings')), []),
    ('build_inspect', 'build.inspect', 'Inspect the saved build profile and CMake File API target/toolchain metadata without executing project code.', schema(BUILD_FIELDS), BUILD_PROJECT, []),
    ('build_configure', 'build.configure', 'Configure a trusted CMake project using a detected toolchain. Executes project CMake code, requests File API metadata, prepares clangd compilation flags and saves the active workspace profile on success. Refuses to switch generator/compiler in an existing build tree.', schema(BUILD_FIELDS), BUILD_RESULT, ['process.execute']),
    ('build_run', 'build.run', 'Build an already configured trusted CMake project or one configured target. Uses the saved profile by default; saves the selected configuration on success.', schema(BUILD_FIELDS), BUILD_RESULT, ['process.execute']),
    ('plugins_list', 'plugins.list', 'List loaded native plugins, commands and revision-control providers.', schema(),
     schema({'abi': {'type': 'integer'}, 'plugins': ARRAY, 'commands': ARRAY, 'vcsProviders': ARRAY}, ('abi', 'plugins', 'commands', 'vcsProviders')), []),
    ('plugins_invoke', 'plugins.invoke', 'Execute a registered command from a trusted native plugin.', schema({'command': STRING, 'input': {'type': 'object'}}, ('command',)), {'type': 'object'}, ['plugins.execute']),
]
METHODS = {operation[0]: operation[1] for operation in OPERATIONS}
for _id, _method, _description, _inputs, _outputs, _permissions in OPERATIONS:
    if _method.startswith('vcs.'):
        _inputs['properties']['provider'] = {'type': 'string', 'description': 'Registered VCS provider ID; defaults to git.'}

def write_manifest():
    path = Path(__file__).with_name('CyTool.json')
    manifest = json.loads(path.read_text('utf-8'))
    manifest.update(description='Native C++ IDE services, shared by CyUI and CyToolsCore MCP.', version='0.1.0',
                    executable='python app/tool.py', standaloneExecutable='CyCode.exe', categories=['development'], tags=['IDE', 'C++', 'plugins'],
                    capabilities=['jobs', 'cancellation', 'live-documents', 'text-search', 'cpp-navigation', 'cpp-symbols', 'cpp-refactoring', 'csharp-language', 'readonly-sdk-sources', 'terminal', 'dap-debugger', 'git', 'native-plugins', 'msbuild'],
                    concurrency={'policy': 'Concurrent', 'maxWorkers': 4}, distributionStatus='Development milestone; see app/Docs/Validation.md')
    for platform in manifest['platforms']:
        platform['status'] = 'Experimental'
    manifest['operations'] = [dict(id=op, name=op.replace('_', ' ').title(), description=description,
        inputSchema=inputs, outputSchema=outputs, resourceRequirements={'ramBytes': 16 * 1024 * 1024},
        supportedBackends=[], permissions=permissions, canRunAsync=True, canCancel=True, supportsProgress=False,
        **({'exclusiveGroups': ['workspace-mutations']} if permissions and method not in ('terminal.list','terminal.read','debug.status','debug.profile') else {}))
        for op, method, description, inputs, outputs, permissions in OPERATIONS]
    path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')

if __name__ == '__main__':
    write_manifest()

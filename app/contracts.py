"""CyCode operations. Generated manifest is the public CyTools v1 contract."""
from pathlib import Path
import json

def schema(properties=None, required=()):
    return {'type': 'object', 'properties': properties or {}, 'required': list(required), 'additionalProperties': False}

STRING = {'type': 'string'}
PATH = {'type': 'string', 'minLength': 1, 'description': 'Workspace reference; live document operations also accept issued untitled references. Not a temporary input artifact. Filesystem operations require actual workspace paths.'}
REVISION = {'type': 'integer', 'minimum': 1}
BOOL = {'type': 'boolean'}
ARRAY = {'type': 'array', 'items': {'type': 'object'}}
DOCUMENT = schema({'path': STRING, 'text': STRING, 'revision': REVISION, 'dirty': BOOL,
                   'id': {'type':'integer','minimum':0},
                   'diskConflict': STRING,
                   'untitled': BOOL, 'displayPath': STRING,
                   'lineEnding': STRING, 'encoding': STRING}, ('path', 'text', 'revision', 'dirty', 'lineEnding', 'encoding'))
RECOVERY_REF = {'session':STRING,'id':STRING,'checkpoint':STRING}
NONNEGATIVE = {'type':'integer','minimum':0}
BUILD_OUTPUT_FIELDS = {'running':BOOL,'cancelling':BOOL,'operationId':{'type':'string','maxLength':128},
    'method':{'type':'string','maxLength':128},'streamId':{'type':'string','maxLength':128},'revision':NONNEGATIVE,
    'changed':BOOL,'byteCount':NONNEGATIVE,'droppedBytes':NONNEGATIVE,'incomplete':BOOL,
    'encoding':{'const':'utf8-repaired'},'output':{'type':'string','maxLength':786432}}
BUILD_OUTPUT = schema(BUILD_OUTPUT_FIELDS,tuple(BUILD_OUTPUT_FIELDS))
BUILD_OUTPUT['allOf']=[{'if':{'properties':{'changed':{'const':False}}},'then':{'properties':{'output':{'const':''}}}},
    {'if':{'properties':{'cancelling':{'const':True}}},'then':{'properties':{'running':{'const':True}}}}]
BUILD_OUTPUT_INPUT = schema({'streamId':{'type':'string','minLength':1,'maxLength':128,'pattern':'^[A-Za-z0-9_.-]+$','not':{'pattern':'[^A-Za-z0-9_.-]'}},'revision':NONNEGATIVE})
BUILD_OUTPUT_INPUT['dependentRequired']={'streamId':['revision'],'revision':['streamId']}
PROJECT_OUTPUT = schema({**BUILD_OUTPUT_FIELDS,'context':schema({'system':STRING,'directory':STRING},('system','directory'))},tuple(BUILD_OUTPUT_FIELDS))
PROJECT_OUTPUT['allOf']=BUILD_OUTPUT['allOf']
COMPILER_LOCATION_INPUT = schema({'text':{'type':'string','maxLength':16384,
    'not':{'pattern':r'[\u0000-\u0008\u000A-\u000C\u000E-\u001F\u007F]'}},
    'directory':{'type':'string','minLength':1,'maxLength':8192}},('text',))
COMPILER_LOCATION = schema({'mapped':BOOL,'reason':STRING,'path':PATH,
    'line':{'type':'integer','minimum':1,'maximum':10000000},
    'column':{'type':'integer','minimum':1,'maximum':1000000},
    'format':{'enum':['colon','parentheses']}},('mapped','reason'))
COMPILER_LOCATION['allOf']=[{'if':{'properties':{'mapped':{'const':True}}},
    'then':{'required':['path','line','column','format'],'properties':{'reason':{'const':''}}},
    'else':{'not':{'anyOf':[{'required':[key]} for key in ('path','line','column','format')]}}}]
TASK_STATUS = schema({'running':BOOL,'operationId':STRING,'method':STRING,'cancelling':BOOL},('running','operationId','method','cancelling'))
RECOVERY_STATUS = schema({'session':STRING,'documents':NONNEGATIVE,'bytes':NONNEGATIVE,'lastCheckpoint':NONNEGATIVE,
    'maxDocuments':NONNEGATIVE,'maxBytes':NONNEGATIVE,'pending':NONNEGATIVE,'errors':ARRAY,'intervalMs':NONNEGATIVE},
    ('session','documents','bytes','lastCheckpoint','maxDocuments','maxBytes','pending','errors','intervalMs'))
RECOVERY_ITEM = schema({**RECOVERY_REF,'path':PATH,'revision':REVISION,'updatedAt':NONNEGATIVE,'bytes':NONNEGATIVE,'baselineBytes':NONNEGATIVE,'active':BOOL},
    ('session','id','checkpoint','path','revision','updatedAt','bytes','baselineBytes','active'))
RECOVERY_PREVIEW = schema({**RECOVERY_ITEM['properties'],'text':STRING,'truncated':BOOL,'diskState':STRING,'diskChanged':BOOL,'missing':BOOL,'targetRevision':NONNEGATIVE,'targetDirty':BOOL},
    (*RECOVERY_ITEM['required'],'text','truncated','diskState','diskChanged','missing','targetRevision','targetDirty'))
RESOURCE_DOCUMENT=schema({'path':PATH,'id':REVISION,'revision':REVISION,'dirty':BOOL,'diskConflict':STRING,'missing':BOOL},('path','id','revision','dirty','diskConflict','missing'))
RESOURCE_SNAPSHOT=schema({'guard':STRING,'entries':NONNEGATIVE,'files':NONNEGATIVE,'bytes':NONNEGATIVE,'directory':BOOL},('guard','entries','files','bytes','directory'))
RESOURCE_EFFECT=schema({'from':STRING,'to':STRING,'removed':BOOL,'documents':ARRAY,'warnings':{'type':'array','items':STRING},'revision':REVISION},('from','to','removed','documents','warnings','revision'))
RESOURCE_PLAN=schema({'id':STRING,'state':{'type':'string','enum':['preview','applied','undone']},'createdAt':NONNEGATIVE,
    'operation':{'type':'string','enum':['createFile','createDirectory','move','delete','restore']},'path':STRING,'destination':STRING,'trash':STRING,
    'openDocuments':{'type':'array','items':RESOURCE_DOCUMENT},'blocked':BOOL,'warning':STRING,'snapshot':RESOURCE_SNAPSHOT,
    'trashGuard':STRING,'expectedTrashContent':STRING,'afterGuard':STRING,'appliedAt':NONNEGATIVE,'effect':RESOURCE_EFFECT,'undone':STRING},
    ('id','state','createdAt','operation','path','destination','trash','openDocuments','blocked','warning','snapshot'))
TEMPLATE_OPTIONS={'type':'object','maxProperties':16,'additionalProperties':{'anyOf':[STRING,BOOL]}}
TEMPLATE_FIELD=schema({'id':STRING,'title':STRING,'type':{'type':'string','enum':['value','identifier','choice','boolean']},'default':{'anyOf':[STRING,BOOL]},'choices':{'type':'array','maxItems':32,'items':STRING},'help':STRING},('id','title','type','default'))
TEMPLATE_INFO=schema({'id':STRING,'title':STRING,'language':STRING,'buildSystem':STRING,'requirements':{'type':'array','items':STRING},'plugin':STRING,'nameStyle':STRING,'fields':{'type':'array','items':TEMPLATE_FIELD}},('id','title','language','buildSystem','requirements','plugin','nameStyle','fields'))
TEMPLATE_PLAN=schema({'id':STRING,'template':STRING,'name':STRING,'destination':STRING,'entry':STRING,'openFile':STRING,'framework':STRING,
    'plugin':STRING,'catalogueRevision':NONNEGATIVE,'options':TEMPLATE_OPTIONS,
    'files':{'type':'array','items':schema({'path':STRING,'text':STRING,'bytes':NONNEGATIVE},('path','text','bytes'))},
    'requirements':{'type':'array','items':STRING},'instructions':STRING,'state':{'type':'string','enum':['preview','created']}},
    ('id','template','name','destination','entry','openFile','framework','files','requirements','instructions','state'))
PROCESS = schema({'exitCode': {'type': 'integer'}, 'output': STRING, 'timedOut': BOOL, 'truncated': BOOL, 'cancelled': BOOL},
                 ('exitCode', 'output', 'timedOut', 'truncated'))
VCS_RESULT = schema({**PROCESS['properties'], 'reloaded': {'type':'array','items':PATH}, 'retained': {'type':'array','items':PATH}}, PROCESS['required'])
VCS_INSPECT = schema({'repository':BOOL,'root':STRING,'branch':STRING,'head':STRING,'branches':ARRAY,'remotes':ARRAY,'stashes':ARRAY,
                      'ahead':{'type':'integer'},'behind':{'type':'integer'},'hasUpstream':BOOL},
                     ('repository','root','branch','head','branches','remotes','stashes','ahead','behind','hasUpstream'))
BUILD_FIELDS = {'source': PATH, 'buildDirectory': PATH,
                'configuration': {'type': 'string', 'minLength':1,'maxLength':128,'description':'Manual mode supports the four standard configurations. A preset may define a custom configuration.'},
                'toolchain': {'type': 'string', 'description': 'ID from toolchains_detect, or automatic.'},
                'generator': {'type': 'string', 'description': 'CMake generator name, or automatic.'},
                'target': {'type': 'string', 'description': 'Configured CMake target override; empty preserves build preset targets or default targets.'},
                'configurePreset': {'type':'string','maxLength':160,'description':'Name from build_presets; empty selects manual configuration.'},
                'buildPreset': {'type':'string','maxLength':160,'description':'Optional native build preset. Its configurePreset must match the selected configure preset.'}}
CMAKE_PRESETS = schema({'configurePresets':ARRAY,'buildPresets':ARRAY,'testPresets':ARRAY,'packagePresets':ARRAY,
    'workflowPresets':ARRAY,'revision':STRING,'files':{'type':'array','items':PATH},'error':STRING},('configurePresets','buildPresets','files','error'))
PRESET_ACTION_FIELDS = {'source':PATH,'kind':{'type':'string','enum':['test','package','workflow']},
    'preset':{'type':'string','minLength':1,'maxLength':160}}
PRESET_STEP = schema({'type':{'type':'string','enum':['configure','build','test','package']},'name':STRING,
    'configurePreset':STRING,'buildDirectory':PATH,'configuration':STRING,'configurations':{'type':'array','maxItems':64,'items':STRING},'outputs':{'type':'array','items':PATH}},
    ('type','name','configurePreset','buildDirectory','configuration','configurations','outputs'))
PRESET_PLAN = schema({**PRESET_ACTION_FIELDS,'configurePreset':STRING,'buildDirectory':PATH,'generator':STRING,
    'configuration':STRING,'configurations':{'type':'array','maxItems':64,'items':STRING},'steps':{'type':'array','minItems':1,'maxItems':64,'items':PRESET_STEP},
    'outputs':{'type':'array','items':PATH},'program':{'type':'string','enum':['cmake','ctest','cpack']},
    'arguments':{'type':'array','items':STRING},'minimumCMake':STRING,
    'revision':{'type':'string','minLength':1,'maxLength':256}},
    ('source','kind','preset','configurePreset','buildDirectory','generator','configuration','configurations','steps','outputs','program','arguments','minimumCMake','revision'))
BUILD_PROJECT = schema({'profile': schema(BUILD_FIELDS, tuple(BUILD_FIELDS)), 'configured': BOOL, 'generator': STRING,
                        'presets':CMAKE_PRESETS,
                        'configurations': {'type': 'array', 'items': STRING}, 'targets': ARRAY, 'toolchains': ARRAY,
                        'compilationDatabase': schema({'path': STRING, 'origin': STRING, 'entries': {'type': 'integer', 'minimum': 0}, 'available': BOOL}, ('path', 'origin', 'entries', 'available')),
                        'message': STRING}, ('profile', 'configured', 'generator', 'configurations', 'targets', 'toolchains', 'compilationDatabase', 'message'))
BUILD_RESULT = schema({**PROCESS['properties'], 'project': {'anyOf': [BUILD_PROJECT, {'type': 'null'}]}}, (*PROCESS['required'], 'project'))
PRESET_RESULT = schema({**BUILD_RESULT['properties'],'plan':PRESET_PLAN,'inspectionError':STRING},(*BUILD_RESULT['required'],'plan'))
PROJECT_FIELDS = {'system':{'type':'string','enum':['automatic','cargo','npm','java','maven','gradle','shader']},'directory':PATH,
    'configuration':{'type':'string','enum':['Debug','Release']},'package':STRING,'target':STRING,'script':STRING,
    'mainClass':STRING,'sourceDirectory':PATH,'offline':BOOL,'timeoutSeconds':{'type':'integer','minimum':1,'maximum':86400},'args':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':8192}}}
RUST_TOOLCHAIN = {'type':'string','maxLength':128,'pattern':'^([A-Za-z0-9][A-Za-z0-9_.-]*)?$','not':{'pattern':'[^A-Za-z0-9_.-]'},'description':'Empty keeps automatic selection; otherwise an existing rustup toolchain name, not a path. No installation requested.'}
PROJECT_FIELDS.update({'cargoToolchain':RUST_TOOLCHAIN,'cargoWorkspace':BOOL,
    'cargoTargetKind':{'type':'string','enum':['bin','example','lib','test','bench']},
    'cargoProfile':{'type':'string','maxLength':128,'description':'Optional Cargo --profile; overrides Debug/Release. Empty retains Configuration.'},
    'cargoFeatures':{'type':'array','maxItems':128,'uniqueItems':True,'items':{'type':'string','minLength':1,'maxLength':256}},
    'cargoAllFeatures':BOOL,'cargoNoDefaultFeatures':BOOL,'cargoLocked':BOOL})
RUST_ANALYSIS = schema({'toolchain':RUST_TOOLCHAIN,'manifest':{'type':'string','minLength':1,'maxLength':8192,'description':'Workspace-relative Cargo.toml. Resolved and symlink-guarded on explicit configuration.'},
    'features':PROJECT_FIELDS['cargoFeatures'],'allFeatures':BOOL,'noDefaultFeatures':BOOL,'offline':BOOL,'locked':BOOL},('manifest',))
PROJECT_FIELDS.update({'shaderSource':PATH,
    'shaderLanguage':{'type':'string','enum':['automatic','hlsl','glsl']},
    'shaderStage':{'type':'string','enum':['automatic','vert','frag','geom','tesc','tese','comp','mesh','task','rgen','rint','rahit','rchit','rmiss','rcall']},
    'shaderTarget':{'type':'string','enum':['automatic','dxil','vulkan1.0','vulkan1.1','vulkan1.2','vulkan1.3','opengl']},
    'shaderEntry':{'type':'string','minLength':1,'maxLength':128},'shaderModel':{'type':'string','pattern':'^6_[0-9]$'},
    'shaderDefines':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':4096}},
    'shaderIncludes':{'type':'array','maxItems':64,'items':PATH}})
CARGO_PACKAGE = schema({'id':STRING,'name':STRING,'manifest':PATH,'defaultMember':BOOL,'features':{'type':'array','maxItems':2000,'items':STRING}},
    ('id','name','manifest','defaultMember','features'))
PROJECT_MODEL = schema({'available':BOOL,'kind':STRING,'name':STRING,'profile':{'type':'object'},'tools':{'type':'object'},
    'workspaceRoot':STRING,'packages':{'type':'array','maxItems':2000,'items':CARGO_PACKAGE},
    'rustAnalysis':RUST_ANALYSIS,
    'targets':ARRAY,'scripts':ARRAY,'sources':{'type':'array','items':STRING},'artifacts':ARRAY,'configured':BOOL,'stale':BOOL,'message':STRING},
    ('available','kind','name','profile','tools','targets','scripts','sources','artifacts','configured','stale','message'))
PROJECT_RESULT = schema({**PROCESS['properties'],'cancelled':BOOL,'project':PROJECT_MODEL,'diagnostics':ARRAY,'terminal':{'type':'object'},'program':STRING},
    (*PROCESS['required'],'cancelled','project','diagnostics'))
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
RELATED_INDEX = schema({'state':{'type':'string','enum':['missing','busy','stale','ready','partial','changed']},
                        'revision':NONNEGATIVE,'generation':NONNEGATIVE},('state','revision','generation'))
HIERARCHY_ID = {'type':'string','minLength':1,'maxLength':128}
HIERARCHY_KIND = {'type':'string','enum':['call','type']}
HIERARCHY_LOCATION = schema({'path':PATH,'range':RANGE,'external':BOOL,'displayPath':STRING},('path','range'))
HIERARCHY_ITEM = schema({**HIERARCHY_LOCATION['properties'],'id':HIERARCHY_ID,
    'name':STRING,'detail':STRING,'kind':{'type':'integer','minimum':1,'maximum':26}},
    ('path','range','id','name','detail','kind'))
HIERARCHY_NODE = schema({'item':HIERARCHY_ITEM,'sites':{'type':'array','maxItems':256,'items':HIERARCHY_LOCATION}},('item','sites'))
HIERARCHY_RESULT = schema({'sessionId':HIERARCHY_ID,'path':PATH,'revision':REVISION,'kind':HIERARCHY_KIND,
    'nodes':{'type':'array','maxItems':256,'items':HIERARCHY_NODE},'truncated':BOOL,'omitted':NONNEGATIVE},
    ('sessionId','path','revision','kind','nodes','truncated','omitted'))
INFO_SECTION = schema({'kind':{'type':'string','enum':['plaintext','markdown','code']},'language':STRING,
                       'text':{'type':'string','maxLength':65536}},('kind','language','text'))
INFO_SECTIONS = {'type':'array','maxItems':32,'items':INFO_SECTION}
INFO_FIELDS = {'path':PATH,'revision':REVISION,'positionEncoding':{'const':'utf-16'},'available':BOOL,'truncated':BOOL}
INFO_PARAMETER = schema({'label':STRING,'startByte':{'type':'integer','minimum':-1},'endByte':{'type':'integer','minimum':-1},
    'documentation':INFO_SECTIONS},('label','startByte','endByte','documentation'))
INFO_SIGNATURE = schema({'label':STRING,'documentation':INFO_SECTIONS,
    'parameters':{'type':'array','maxItems':128,'items':INFO_PARAMETER},'activeParameter':{'type':'integer','minimum':-1,'description':'-1 means the server did not report an active parameter, or the signature has none.'}},
    ('label','documentation','parameters','activeParameter'))
NAVIGATION_METHODS = ['language.definition','language.declaration','language.typeDefinition','language.implementation','language.references']
LOCATIONS = schema({'path':PATH,'revision':REVISION,'documentId':NONNEGATIVE,'backend':STRING,'epoch':NONNEGATIVE,
    'method':{'type':'string','enum':NAVIGATION_METHODS},'position':POSITION,'positionEncoding':{'const':'utf-16'},
    'locations':{'type':'array','maxItems':5000,'items':schema({'path':PATH,'range':RANGE,'fullRange':RANGE,'originRange':RANGE,'external':BOOL,'displayPath':STRING},('path','range','fullRange'))},
    'outsideWorkspace':NONNEGATIVE,'invalid':NONNEGATIVE,'truncated':BOOL,'cacheHit':BOOL,'astReconciled':BOOL},
    ('path','revision','documentId','backend','epoch','method','position','positionEncoding','locations','outsideWorkspace','invalid','truncated'))
def navigation_output(method):
    return {**LOCATIONS,'properties':{**LOCATIONS['properties'],'method':{'const':method}}}
OCCURRENCES = schema({'path':PATH,'revision':REVISION,'documentId':NONNEGATIVE,'backend':STRING,'epoch':NONNEGATIVE,
    'position':POSITION,'positionEncoding':{'const':'utf-16'},
    'occurrences':{'type':'array','maxItems':4096,'items':schema({'range':RANGE,'kind':{'type':'integer','minimum':1,'maximum':3}},('range','kind'))},
    'invalid':NONNEGATIVE,'truncated':BOOL},
    ('path','revision','documentId','backend','epoch','position','positionEncoding','occurrences','invalid','truncated'))
LANGUAGE_INPUT = {'path': PATH, 'expectedRevision': REVISION, 'position': POSITION,
                  'includeExternal': BOOL,'refresh':BOOL,
                  'limit': {'type': 'integer', 'minimum': 1, 'maximum': 5000}}
LANGUAGE_NAV_INPUT = {**LANGUAGE_INPUT,'expectedDocumentId':NONNEGATIVE,'expectedEpoch':NONNEGATIVE}
BACKENDS = ['cpp','csharp','rust','java','typescript','html','css','python','hlsl','glsl']
LANGUAGE_LAUNCH = {'arguments':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':8192}},
                   'settings':{'type':'object'},'executable':{'type':'string','maxLength':8192},
                   'cargoProject':{'anyOf':[RUST_ANALYSIS,{'type':'null'}],
                     'description':'Rust only: apply a typed manifest/feature/resolution overlay without replacing raw argv/settings. project_inspect.rustAnalysis supplies this object; null removes it. Restarts/enables only through this explicit process-authorized operation. Package/target/custom profiles do not map to analysis.'}}
PREFERENCES_REVISION = {'type':'integer','minimum':0,'maximum':9007199254740991}
LANGUAGE_PREFERENCES = schema({'available':BOOL,'revision':PREFERENCES_REVISION,'error':STRING,
    'profiles':{'type':'object','maxProperties':42,'additionalProperties':schema({**LANGUAGE_LAUNCH,'enabled':BOOL},('arguments','settings','enabled'))}},
    ('available','revision','error','profiles'))
LANGUAGE_PREFERENCES_STATUS = schema({'available':BOOL,'revision':PREFERENCES_REVISION,'error':STRING,
    'backends':{'type':'object','maxProperties':42,'additionalProperties':BOOL}},('available','revision','error','backends'))
LANGUAGE_SERVER = schema({'ready':BOOL, 'running':BOOL, 'error':STRING,'logs':STRING, 'diagnostics':{'type':'object'}, 'name':STRING, 'enabled':BOOL, 'available':BOOL,'plugin':STRING,'languages':ARRAY,'arguments':{'type':'array','items':STRING},'settings':{'type':'object'},
                         'diagnosticGeneration':{'type':'integer','minimum':0}, 'epoch':NONNEGATIVE,
                         'configured':BOOL,'initialized':BOOL,'launchAttempted':BOOL,'state':STRING,'compilationDatabase':STRING,
                         'synchronizedRevisions':{'type':'object','additionalProperties':schema({'revision':REVISION,'id':NONNEGATIVE,'lspVersion':REVISION},('revision','id','lspVersion'))},
                         'analysis':schema({'backend':STRING,'projectIndexCompleteness':STRING,'openDocuments':NONNEGATIVE,'synchronizedDocuments':NONNEGATIVE,'versionBoundDiagnosticDocuments':NONNEGATIVE,'synchronizationMeaning':STRING},
                            ('backend','projectIndexCompleteness')),
                         'diagnosticRevisions':{'type':'object','additionalProperties':schema({'revision':REVISION,'id':NONNEGATIVE,'uri':STRING},('revision','id','uri'))},
                         'configuration':schema(LANGUAGE_LAUNCH,('arguments','settings')),
                         'methods':schema({key:BOOL for key in ['completion','completionResolve','codeActions','codeActionResolve','hover','signature','definition','declaration','typeDefinition','implementation','references','documentHighlights','symbols','symbolResolve','documentSymbols','callHierarchy','typeHierarchy','rename','format','rangeFormat']}),
                         'semanticTokens':BOOL,'semanticGeneration':{'type':'integer','minimum':0}},
                         ('ready','running','error','diagnostics','name','enabled'))
LANGUAGE_STATUS = schema({**LANGUAGE_SERVER['properties'], 'preferences':LANGUAGE_PREFERENCES_STATUS, 'servers':{'type':'object','properties':{key:LANGUAGE_SERVER for key in BACKENDS},'required':BACKENDS,'additionalProperties':LANGUAGE_SERVER}},
                        (*LANGUAGE_SERVER['required'],'servers'))
SYMBOL = schema({'path': PATH, 'range': RANGE, 'external': BOOL, 'displayPath': STRING,
                 'name': STRING, 'kind': {'type': 'integer','minimum':1,'maximum':26}, 'container': STRING, 'detail': STRING,
                 'node':REVISION,'parent':NONNEGATIVE,'depth':{'type':'integer','minimum':0,'maximum':64},
                 'fullRange':RANGE,'deprecated':BOOL},
                ('path', 'range', 'name', 'kind', 'container', 'detail','node','parent','depth','fullRange','deprecated'))
SYMBOL_PENDING = schema({key:value for key,value in SYMBOL['properties'].items() if key not in ('range','fullRange')},
                        tuple(key for key in SYMBOL['required'] if key not in ('range','fullRange')))
SYMBOL_PENDING['properties'].update({'unresolved':{'const':True},'resolveIndex':{'type':'integer','minimum':0,'maximum':999}})
SYMBOL_PENDING['required'] += ['unresolved','resolveIndex']
SYMBOL_SESSION = {'type':'string','pattern':r'^ws-symbol-[1-9][0-9]{0,19}$','maxLength':40,'not':{'pattern':r'[\r\n]'}}
SYMBOLS = schema({'symbols': {'type': 'array','maxItems':1000, 'items': {'oneOf':[SYMBOL,SYMBOL_PENDING]}}, 'outsideWorkspace': {'type': 'integer', 'minimum': 0},
                  'truncated': BOOL, 'positionEncoding': {'type': 'string', 'const': 'utf-16'}, 'path': PATH, 'revision': REVISION,
                  'documentId':NONNEGATIVE,'backend':STRING,'epoch':NONNEGATIVE,'invalid':NONNEGATIVE,'session':SYMBOL_SESSION},
                 ('symbols', 'outsideWorkspace', 'invalid', 'truncated', 'positionEncoding','backend','epoch'))
SYMBOLS['dependentRequired']={'path':['revision','documentId'],'revision':['path','documentId'],'documentId':['path','revision']}
SYMBOLS['allOf']=[{'if':{'properties':{'symbols':{'contains':SYMBOL_PENDING}},'required':['symbols']},'then':{'required':['session']}}]
DOCUMENT_SYMBOLS = dict(SYMBOLS, properties={**SYMBOLS['properties'], 'symbols':{'type':'array','maxItems':1000,'items':SYMBOL}})
ANNOTATION_ID = {'type':'string','minLength':1,'maxLength':256}
CHANGE_ANNOTATIONS = {'type':'array','maxItems':128,'items':schema({
    'id':ANNOTATION_ID,'label':{'type':'string','minLength':1,'maxLength':2048},
    'description':{'type':'string','maxLength':4096},'needsConfirmation':BOOL,
    'files':{'type':'array','maxItems':100,'items':PATH}},('id','label','description','needsConfirmation','files'))}
CHANGE = schema({'id': STRING, 'title': STRING, 'state': {'type': 'string', 'enum': ['preview', 'applied', 'undone']},
                 'files': {'type': 'array', 'items': schema({'path': PATH, 'expectedRevision': REVISION, 'editCount': {'type': 'integer', 'minimum': 0}}, ('path', 'expectedRevision', 'editCount'))},
                 'editCount': {'type': 'integer', 'minimum': 0},'annotations':CHANGE_ANNOTATIONS,
                 'coverage':schema({'strategy':STRING,'preparedFiles':NONNEGATIVE,'wholeProjectIndexCompleteness':{'const':'unknown'},'limitations':{'type':'array','items':STRING}},('strategy','preparedFiles','wholeProjectIndexCompleteness','limitations'))}, ('id', 'title', 'state', 'files', 'editCount'))
USER_SNIPPETS = schema({'available':BOOL,'editable':BOOL,'valid':BOOL,'revision':STRING,
    'generation':NONNEGATIVE,'text':{'type':'string','maxLength':262144},'error':STRING},
    ('available','editable','valid','revision','generation','text','error'))
USER_SNIPPET_REVISION = {'type':'string','anyOf':[{'const':'absent'},
    {'minLength':64,'maxLength':64,'pattern':'^[0-9a-f]{64}$'}]}
SNIPPET = schema({'positionEncoding':{'type':'string','const':'utf-8-bytes'},'final':{'type':'integer','minimum':0},
                  'fields':{'type':'array','maxItems':128,'items':schema({'index':{'type':'integer','minimum':1,'maximum':65535},
                      'ranges':{'type':'array','minItems':1,'maxItems':128,'items':schema({'start':{'type':'integer','minimum':0},'end':{'type':'integer','minimum':0},'parents':{'type':'array','maxItems':16,'items':{'type':'integer','minimum':0,'maximum':65535}}},('start','end','parents'))},
                      'choices':{'type':'array','maxItems':64,'items':STRING}},('index','ranges','choices'))}},('positionEncoding','final','fields'))
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
UNREAL_ARTIFACT_RECEIPT=schema({'path':STRING,'present':BOOL,'targetType':STRING,'buildId':STRING})
UNREAL_ARTIFACT_ISSUE=schema({'code':{'type':'string','maxLength':128},'path':{'type':'string','maxLength':8192},'detail':{'type':'string','maxLength':16384}},('code','path','detail'))
UNREAL_ARTIFACTS=schema({'supported':BOOL,'complete':BOOL,'ready':BOOL,'receiptRequired':BOOL,
    'scope':{'const':'projectEditorOutputs'},'revision':{'type':'string','minLength':1,'maxLength':256},
    'profile':schema(UNREAL_FIELDS,tuple(UNREAL_FIELDS)), 'receipt':UNREAL_ARTIFACT_RECEIPT,'engineManifest':UNREAL_ARTIFACT_RECEIPT,
    'products':{'type':'array','maxItems':4096,'items':schema({'path':STRING,'type':{'enum':['DynamicLibrary','Executable']},'present':BOOL},('path','type','present'))},
    'manifests':{'type':'array','maxItems':256,'items':schema({'path':STRING,'buildId':STRING,'moduleCount':{'type':'integer','minimum':0,'maximum':4096},'matchesEngine':BOOL},('path','buildId','moduleCount','matchesEngine'))},
    'modules':{'type':'array','maxItems':4096,'items':schema({'name':STRING,'path':STRING,'manifest':STRING,'present':BOOL},('name','path','manifest','present'))},
    'issues':{'type':'array','maxItems':128,'items':UNREAL_ARTIFACT_ISSUE}},
    ('supported','complete','ready','receiptRequired','scope','revision','receipt','engineManifest','products','manifests','modules','issues'))
UNREAL_ARTIFACTS['allOf']=[{'if':{'properties':{'ready':{'const':True}},'required':['ready']},
    'then':{'properties':{'supported':{'const':True},'complete':{'const':True},'issues':{'maxItems':0}},'required':['profile']}}]
UNREAL_RESULT = schema({**PROCESS['properties'], 'project': UNREAL_MODEL, 'cached': BOOL,
    'artifacts':UNREAL_ARTIFACTS,'inspectionError':STRING}, (*PROCESS['required'], 'project'))
UNREAL_AUTOMATION_MAP={'type':'string','minLength':7,'maxLength':512,
    'pattern':'^/Game/(?:[^/]+/)*[^/]+$','not':{'pattern':'[\u0000-\u002e\u003a-\u0040\u005b-\u005e\u0060\u007b-\u007f]'}}
UNREAL_AUTOMATION_PATH={'type':'string','minLength':1,'maxLength':8192,'not':{'pattern':'[\u0000-\u001f\u007f]'}}
UNREAL_AUTOMATION_TOKEN={'type':'string','minLength':1,'maxLength':128,'pattern':'^[A-Za-z0-9_.-]+$','not':{'pattern':'[\u0000-\u0020\u007f]'}}
UNREAL_AUTOMATION_FIELDS={**UNREAL_FIELDS,'project':UNREAL_AUTOMATION_PATH,
    'engineRoot':{'type':'string','maxLength':8192,'not':{'pattern':'[\u0000-\u001f\u007f]'}},
    'target':{'type':'string','minLength':1,'maxLength':128,'pattern':'^[A-Za-z_][A-Za-z0-9_]*$',
              'not':{'anyOf':[{'pattern':'Editor$'},{'pattern':'[^A-Za-z0-9_]'}]}},
    'platform':{'type':'string','enum':['Win64','Linux','LinuxArm64','Mac']},
    'action':{'type':'string','enum':['cook','package']},'maps':{'type':'array','maxItems':64,'uniqueItems':True,'items':UNREAL_AUTOMATION_MAP},
    'build':BOOL,'pak':BOOL,'ioStore':BOOL,'compressed':BOOL,'timeoutSeconds':{'type':'integer','minimum':1,'maximum':86400}}
UNREAL_AUTOMATION_RULES=[{'if':{'properties':{'action':{'const':'package'},'ioStore':{'const':True}},'required':['action','ioStore']},
    'then':{'properties':{'pak':{'const':True}}}},
    {'if':{'properties':{'action':{'const':'package'},'compressed':{'const':True}},'required':['action','compressed']},
    'then':{'properties':{'pak':{'const':True}}}}]
UNREAL_AUTOMATION_REQUEST=schema(UNREAL_AUTOMATION_FIELDS,tuple(UNREAL_AUTOMATION_FIELDS))
UNREAL_AUTOMATION_REQUEST['allOf']=[*UNREAL_AUTOMATION_RULES,{'if':{'properties':{'action':{'const':'cook'}}},
    'then':{'properties':{'pak':{'const':False},'ioStore':{'const':False},'compressed':{'const':False}}}}]
UNREAL_AUTOMATION_PLAN_FIELDS={'id':UNREAL_AUTOMATION_TOKEN,'revision':UNREAL_AUTOMATION_TOKEN,'request':UNREAL_AUTOMATION_REQUEST,
    'arguments':{'type':'array','minItems':12,'maxItems':32,'items':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}}},
    'cwd':UNREAL_AUTOMATION_PATH,'outputDirectory':UNREAL_AUTOMATION_PATH,'archiveDirectory':STRING,'stagingDirectory':STRING,
    'logDirectory':UNREAL_AUTOMATION_PATH,'cooksContent':{'const':True},'packages':BOOL,'buildsCode':BOOL,
    'executesProjectCode':{'const':True},'mayWriteEngineCaches':{'const':True},'launchesPackagedGame':{'const':False}}
UNREAL_AUTOMATION_PLAN=schema(UNREAL_AUTOMATION_PLAN_FIELDS,tuple(UNREAL_AUTOMATION_PLAN_FIELDS))
UNREAL_AUTOMATION_RUN_FIELDS={'planId':UNREAL_AUTOMATION_TOKEN,'expectedRevision':UNREAL_AUTOMATION_TOKEN,
    'timeoutSeconds':UNREAL_AUTOMATION_FIELDS['timeoutSeconds']}
UNREAL_AUTOMATION_RESULT=schema({**PROCESS['properties'],'cancelled':BOOL,'outputRepaired':BOOL,
    'planId':UNREAL_AUTOMATION_TOKEN,'outputDirectory':UNREAL_AUTOMATION_PATH,'archiveDirectory':STRING,'logDirectory':UNREAL_AUTOMATION_PATH,
    'archiveAvailable':BOOL,'project':UNREAL_MODEL,'inspectionError':STRING,'receiptError':STRING},
    (*PROCESS['required'],'cancelled','outputRepaired','planId','outputDirectory','archiveDirectory','logDirectory','archiveAvailable','project'))
ENVIRONMENT = {'type':'object','maxProperties':128,'additionalProperties':{'type':['string','null']}}
JAVA_ATTACH_HOST = {'type':'string','minLength':1,'maxLength':253,
    'not':{'pattern':r'[^A-Za-z0-9_.:%-]'},
    'allOf':[{'if':{'pattern':':'},
              'then':{'allOf':[{'pattern':r'^[0-9A-Fa-f:.]+(%[A-Za-z0-9_.-]+)?$'},{'pattern':r':.*:'}]},
              'else':{'not':{'pattern':'%'}}}]}
JAVA_ATTACH = schema({'hostName':JAVA_ATTACH_HOST,
    'port':{'type':'integer','minimum':1,'maximum':65535},
    'timeout':{'type':'integer','minimum':100,'maximum':10000,'description':'JDWP connect timeout in milliseconds.'}},
    ('hostName','port','timeout'))
JAVA_DEBUG_TEXT = {'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}}
JAVA_DEBUG_OPTIONS = schema({
    'mainClass':{'type':'string','maxLength':4096,'not':{'pattern':r'[\x00-\x20\x7f/\\:;]'}},
    'projectName':JAVA_DEBUG_TEXT,'javaExec':JAVA_DEBUG_TEXT,
    **{key:{'type':'array','maxItems':128,'items':{**JAVA_DEBUG_TEXT,'minLength':1}}
       for key in ('classPaths','sourcePaths')},
    'vmArgs':{'type':'array','maxItems':128,'items':JAVA_DEBUG_TEXT}})
DEBUG_FIELDS = {'name':STRING,'program':{'type':'string','maxLength':4096},'cwd':PATH,'args':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':8192}},
                'adapterId':{'type':'string','minLength':1,'maxLength':160},'adapterOptions':{'type':'object','maxProperties':128,'description':'Adapter-specific DAP parameters; common request/type/cwd/program/args/env/stopOnEntry/pid fields remain host-owned. Native JSON/size limits apply.'},
                'env':ENVIRONMENT,'adapter':STRING,'adapterArgs':{'type':'array','maxItems':128,'items':{'type':'string','maxLength':8192}},'runtime':{'type':'string','enum':['automatic','native','dotnet']},'dotnet':STRING,'stopOnEntry':BOOL,'terminal':{'type':'string','enum':['integrated','console','external']},
                'watches':{'type':'array','maxItems':32,'items':{'type':'string','maxLength':8192}},'buildBeforeLaunch':BOOL,
                'javaAttach':JAVA_ATTACH}
def debug_schema(required=()):
    value=schema(dict(DEBUG_FIELDS),required)
    value['allOf']=[{'if':{'properties':{'adapterId':{'const':'builtin.java'}},'required':['adapterId']},
                     'then':{'properties':{'adapterOptions':JAVA_DEBUG_OPTIONS}}}]
    return value
DEBUG_PROFILE_REQUIRED = tuple(key for key in DEBUG_FIELDS if key != 'javaAttach')
UNREAL_LAUNCH_OPTIONS = {
    'mode':{'type':'string','enum':['editor','game','server']},
    'map':{'anyOf':[{'const':''},UNREAL_AUTOMATION_MAP]},
    'listen':BOOL,'log':BOOL,'windowed':BOOL,
    'width':{'type':'integer','minimum':320,'maximum':16384},
    'height':{'type':'integer','minimum':200,'maximum':16384},
    'port':{'type':'integer','minimum':1,'maximum':65535},
    'stopOnEntry':BOOL,'buildBeforeLaunch':BOOL}
UNREAL_LAUNCH_RULES = [{'if':{'properties':{'listen':{'const':True}},'required':['listen']},
    'then':{'properties':{'mode':{'const':'game'},'map':UNREAL_AUTOMATION_MAP},'required':['mode','map']}}]
UNREAL_LAUNCH_INPUT = schema({**UNREAL_FIELDS,**UNREAL_LAUNCH_OPTIONS,
    'configuration':{'type':'string','enum':['Development','DebugGame']},
    'platform':{'type':'string','enum':['Win64','Linux','LinuxArm64','Mac']}})
UNREAL_LAUNCH_INPUT['allOf'] = UNREAL_LAUNCH_RULES
UNREAL_LAUNCH_REQUEST = schema(UNREAL_LAUNCH_OPTIONS, tuple(UNREAL_LAUNCH_OPTIONS))
UNREAL_LAUNCH_REQUEST['allOf'] = UNREAL_LAUNCH_RULES
UNREAL_LAUNCH_BINDING = schema({'profile':schema({key:UNREAL_LAUNCH_INPUT['properties'][key] for key in UNREAL_FIELDS},tuple(UNREAL_FIELDS)),
    'request':UNREAL_LAUNCH_REQUEST},('profile','request'))
DEBUG_FIELDS['unrealLaunch'] = {'anyOf':[{'type':'null'},UNREAL_LAUNCH_BINDING],
    'description':'Optional persisted Unreal build/launch binding. Prepared settings include it. Native configure/run/launch validate its program/cwd/args against the selected engine/project/configuration. Set null explicitly to detach for manual editing. Desktop prebuild uses this binding; SDK launch still does not compile.'}
DEBUG_CONFIGURE_INPUT = debug_schema()
DEBUG_CONFIGURE_INPUT['properties']['expectedProfile'] = debug_schema(DEBUG_PROFILE_REQUIRED)
UNREAL_LAUNCH_SETTING_KEYS = ('name','program','cwd','args','runtime','adapterId','adapter','adapterArgs','adapterOptions','stopOnEntry','buildBeforeLaunch','unrealLaunch')
UNREAL_LAUNCH_SETTINGS = schema({key:DEBUG_FIELDS[key] for key in UNREAL_LAUNCH_SETTING_KEYS},UNREAL_LAUNCH_SETTING_KEYS)
UNREAL_LAUNCH_SETTINGS['properties']['unrealLaunch'] = UNREAL_LAUNCH_BINDING
UNREAL_LAUNCH_RESULT = schema({'request':UNREAL_LAUNCH_REQUEST,'settings':UNREAL_LAUNCH_SETTINGS,
    'unrealProfile':schema(UNREAL_FIELDS,tuple(UNREAL_FIELDS)), 'expectedProfile':debug_schema(DEBUG_PROFILE_REQUIRED),
    'usesUncookedContent':{'const':True},'startsProcess':{'const':False},'savesProfile':{'const':False}},
    ('request','settings','unrealProfile','expectedProfile','usesUncookedContent','startsProcess','savesProfile'))
DEBUG_ATTACH_INPUT = debug_schema()
DEBUG_ATTACH_INPUT['properties']['pid']={'type':'integer','minimum':1,'maximum':2147483647}
DEBUG_ATTACH_INPUT['allOf'].append({
    'if':{'properties':{'adapterId':{'const':'builtin.java'}},'required':['adapterId']},
    'then':{'not':{'required':['pid']}}, 'else':{'required':['pid']}})
PROJECT_RESULT['properties']['debugProfile']=debug_schema(('adapterId','adapterOptions','cwd','args','runtime','program'))
BREAKPOINT = schema({'line':{'type':'integer','minimum':1,'maximum':10000000},'enabled':BOOL,
                     'condition':{'type':'string','maxLength':4096},'hitCondition':{'type':'string','maxLength':4096},'logMessage':{'type':'string','maxLength':4096}},('line',))
BREAKPOINTS = {'type':'array','maxItems':1000,'items':BREAKPOINT}
DATA_HANDLE = {'type':'string','minLength':1,'maxLength':128,'pattern':'^data-[0-9]+-[0-9]+$','not':{'pattern':'[^a-z0-9-]'}}
DATA_TEXT = {'type':'string','maxLength':4096,'not':{'pattern':'\u0000'}}
DATA_ACCESS = {'type':'string','enum':['read','write','readWrite']}
DATA_INFO = schema({'available':BOOL,'id':{'type':'string','maxLength':128},'name':{**DATA_TEXT,'minLength':1},
    'description':{'type':'string','maxLength':8192},'accessTypes':{'type':'array','maxItems':3,'uniqueItems':True,'items':DATA_ACCESS},
    'canPersist':BOOL,'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE},
    ('available','id','name','description','accessTypes','canPersist','sessionGeneration','stopGeneration'))
DATA_INFO['allOf']=[{'if':{'properties':{'available':{'const':True}},'required':['available']},
    'then':{'properties':{'id':DATA_HANDLE}},'else':{'properties':{'id':{'const':''}}}}]
DATA_POINT = schema({'id':DATA_HANDLE,'accessType':DATA_ACCESS,'condition':DATA_TEXT,'hitCondition':DATA_TEXT},('id',))
DATA_POINTS = {'type':'array','maxItems':32,'items':DATA_POINT}
DATA_ACTIVE = schema({**DATA_INFO['properties'],'id':DATA_HANDLE,'available':{'const':True},'accessType':{'type':'string','enum':['','read','write','readWrite']},
    'condition':DATA_TEXT,'hitCondition':DATA_TEXT,
    'verification':schema({'verified':BOOL,'message':{'type':'string','maxLength':8192},'id':{'type':'integer','minimum':0,'maximum':2147483647}},('verified','message'))},
    (*DATA_INFO['required'],'accessType','condition','hitCondition','verification'))
INSTRUCTION_REFERENCE = {'type':'string','minLength':1,'maxLength':8192,'not':{'pattern':'\u0000'}}
INSTRUCTION_POINT = schema({'instructionReference':INSTRUCTION_REFERENCE,
    'offset':{'type':'integer','minimum':-1048576,'maximum':1048576},'enabled':BOOL,
    'condition':DATA_TEXT,'hitCondition':DATA_TEXT},('instructionReference',))
INSTRUCTION_POINTS = {'type':'array','maxItems':128,'items':INSTRUCTION_POINT}
INSTRUCTION_VERIFICATION = schema({'verified':BOOL,'message':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}},
    'id':{'type':'integer','minimum':-2147483648,'maximum':2147483647},'instructionReference':INSTRUCTION_REFERENCE,
    'offset':{'type':'integer','minimum':-9007199254740991,'maximum':9007199254740991},'reason':{'type':'string','enum':['pending','failed']}},('verified','message'))
INSTRUCTION_ACTIVE = schema({**INSTRUCTION_POINT['properties'],'verification':{'anyOf':[INSTRUCTION_VERIFICATION,{'type':'null'}]}},
    (*INSTRUCTION_POINT['properties'],'verification'))
INSTRUCTION_ACTIVE['allOf']=[{'if':{'properties':{'enabled':{'const':True}}},
    'then':{'properties':{'verification':INSTRUCTION_VERIFICATION}},'else':{'properties':{'verification':{'type':'null'}}}}]
DEBUG_MUTATIONS = frozenset(('debug.setVariable','debug.setExpression','debug.writeMemory'))
DEBUG_EXECUTIONS = frozenset(('debug.continue','debug.next','debug.stepIn','debug.stepOut','debug.stepBack','debug.reverseContinue','debug.restartFrame','debug.stepIntoTarget','debug.gotoTarget'))
DEBUG_EXECUTION_CONTEXT = {'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE,
                           'threadId':{'type':'integer','minimum':1,'maximum':2147483647}}
DEBUG_GRANULARITY = {'type':'string','enum':['statement','line','instruction']}
DEBUG_STEP_OPTIONS = {'granularity':DEBUG_GRANULARITY,'singleThread':BOOL}
DEBUG_FRAME_ID = {'type':'integer','minimum':0,'maximum':2147483647}
DEBUG_STEP_TARGET_HANDLE = {'type':'string','minLength':1,'maxLength':96,'not':{'pattern':'\u0000'}}
DEBUG_STEP_TARGET_CONTEXT = {**DEBUG_EXECUTION_CONTEXT,'frameId':DEBUG_FRAME_ID}
DEBUG_STEP_TARGET_POSITION = {'type':'integer','minimum':0,'maximum':9007199254740991}
DEBUG_STEP_TARGET = schema({'id':DEBUG_STEP_TARGET_HANDLE,
    'label':{'type':'string','minLength':1,'maxLength':4096,'not':{'pattern':'\u0000'}},
    **{key:DEBUG_STEP_TARGET_POSITION for key in ('line','column','endLine','endColumn')}},('id','label'))
DEBUG_STEP_TARGET_CATALOGUE = schema({**DEBUG_STEP_TARGET_CONTEXT,'catalogId':DEBUG_STEP_TARGET_HANDLE,
    'targets':{'type':'array','maxItems':128,'items':DEBUG_STEP_TARGET}},
    (*DEBUG_STEP_TARGET_CONTEXT,'catalogId','targets'))
DEBUG_GOTO_COORDINATE = {'type':'integer','minimum':1,'maximum':10000000}
DEBUG_GOTO_SOURCE = schema({'path':{'type':'string','minLength':1,'maxLength':8192,'not':{'pattern':'\u0000'}},
    'name':{'type':'string','minLength':1,'maxLength':4096,'not':{'pattern':'\u0000'}},
    'sourceReference':{'type':'integer','minimum':1,'maximum':2147483647}})
DEBUG_GOTO_SOURCE['anyOf'] = [{'required':['path']},{'required':['sourceReference']}]
DEBUG_GOTO_TARGET = schema({**DEBUG_STEP_TARGET['properties'],
    'instructionPointerReference':{'type':'string','minLength':1,'maxLength':8192,'not':{'pattern':'\u0000'}}},('id','label','line'))
DEBUG_GOTO_CATALOGUE = schema({**DEBUG_STEP_TARGET_CONTEXT,'catalogId':DEBUG_STEP_TARGET_HANDLE,
    'source':DEBUG_GOTO_SOURCE,'requestedLine':DEBUG_GOTO_COORDINATE,'requestedColumn':DEBUG_GOTO_COORDINATE,
    'targets':{'type':'array','maxItems':128,'items':DEBUG_GOTO_TARGET}},
    (*DEBUG_STEP_TARGET_CONTEXT,'catalogId','source','requestedLine','targets'))
DEBUG_EXECUTION_ARGUMENTS = schema({'threadId':DEBUG_EXECUTION_CONTEXT['threadId'],
    'frameId':DEBUG_FRAME_ID,'target':DEBUG_STEP_TARGET_HANDLE,**DEBUG_STEP_OPTIONS})
DEBUG_EXECUTION_RESULT = schema({'allThreadsContinued':BOOL})
DEBUG_EXECUTION_STATE = schema({'method':{'type':'string','enum':sorted(DEBUG_EXECUTIONS)},
    'status':{'type':'string','enum':['applying','accepted','unconfirmed']},
    'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE,'message':STRING,
    'arguments':DEBUG_EXECUTION_ARGUMENTS,'result':DEBUG_EXECUTION_RESULT},
    ('method','status','sessionGeneration','stopGeneration','message','arguments','result'))
DEBUG_EXECUTION_STATE['allOf'] = [
    {'if':{'properties':{'method':{'const':'debug.restartFrame'}}},
     'then':{'properties':{'arguments':{'required':['frameId'],
        'not':{'anyOf':[{'required':[key]} for key in ('threadId','target','singleThread','granularity')]}}}},
     'else':{'if':{'properties':{'method':{'enum':['debug.stepIntoTarget','debug.gotoTarget']}}},
        'then':{'properties':{'arguments':{'required':['threadId','frameId','target']}}},
        'else':{'properties':{'arguments':{'required':['threadId'],
            'not':{'anyOf':[{'required':[key]} for key in ('frameId','target')]}}}}}},
    {'if':{'properties':{'method':{'enum':['debug.continue','debug.reverseContinue']}}},
     'then':{'properties':{'arguments':{'not':{'required':['granularity']}}}},
     'else':{'properties':{'result':{'not':{'required':['allThreadsContinued']}}}}},
    {'if':{'properties':{'method':{'const':'debug.gotoTarget'}}},
      'then':{'properties':{'arguments':{'not':{'anyOf':[{'required':[key]} for key in ('singleThread','granularity')]}}}}}
]
DEBUG_MUTATION_TEXT = {'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}}
DEBUG_MUTATION_TARGET = {**DEBUG_MUTATION_TEXT,'minLength':1,'maxLength':4096}
DEBUG_MUTATION_GUARD = {'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE}
DEBUG_MEMORY_OFFSET = {'type':'integer','minimum':-1048576,'maximum':1048576}
DEBUG_MEMORY_BYTES = {'type':'array','minItems':1,'maxItems':4096,'items':{'type':'integer','minimum':0,'maximum':255}}
DEBUG_MUTATION_RESULT = schema({'value':{**DEBUG_MUTATION_TEXT,'maxLength':65536},'type':DEBUG_MUTATION_TEXT,
    'requestedBytes':{'type':'integer','minimum':1,'maximum':4096},'bytesWritten':{'type':'integer','minimum':0,'maximum':4096},
    'offset':DEBUG_MEMORY_OFFSET,'readBackVerified':BOOL})
DEBUG_MUTATION_STATE = schema({'method':{'type':'string','enum':sorted(DEBUG_MUTATIONS)},
    'status':{'type':'string','enum':['checking','applying','applied','partial','unconfirmed','rejected']},
    'message':STRING,'sessionGeneration':{'type':'integer','minimum':1},'result':DEBUG_MUTATION_RESULT},
    ('method','status','message','sessionGeneration','result'))
DEBUG_MUTATION_STATE['allOf'] = [
    {'if':{'properties':{'method':{'const':'debug.writeMemory'}}},
     'then':{'properties':{'result':{'required':['requestedBytes'],
                                   'not':{'anyOf':[{'required':['value']},{'required':['type']}]}}}},
     'else':{'properties':{'status':{'enum':['applying','applied','unconfirmed','rejected']},
                          'result':{'not':{'anyOf':[{'required':[key]} for key in ('requestedBytes','bytesWritten','offset','readBackVerified')]}}}}},
    {'if':{'properties':{'status':{'const':'applied'},'method':{'const':'debug.writeMemory'}}},
     'then':{'properties':{'result':{'required':['readBackVerified'],'properties':{'readBackVerified':{'const':True}}}}}},
    {'if':{'properties':{'status':{'const':'applied'},'method':{'enum':['debug.setVariable','debug.setExpression']}}},
     'then':{'properties':{'result':{'required':['value']}}}},
    {'if':{'properties':{'status':{'const':'partial'}}},
     'then':{'properties':{'method':{'const':'debug.writeMemory'},'result':{'required':['bytesWritten']}}}}
]
RUN_TO_VERIFICATION = schema({'verified':BOOL,'message':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}},
    'id':{'type':'integer','minimum':-2147483648,'maximum':2147483647},
    'line':DEBUG_STEP_TARGET_POSITION,'column':DEBUG_STEP_TARGET_POSITION,
    'source':schema({'path':{'type':'string','minLength':1,'maxLength':8192,'not':{'pattern':'\u0000'}},
                     'sourceReference':{'type':'integer','minimum':0,'maximum':2147483647}})},('verified','message'))
RUN_TO_STATE = schema({'status':{'type':'string','enum':['idle','arming','running','restoring','stopped','unconfirmed']},
    'revision':NONNEGATIVE,'active':BOOL,'restorePending':BOOL,'replyPending':BOOL,'stopConfirmed':BOOL,
    'sessionGeneration':NONNEGATIVE,'path':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}},
    'line':{'type':'integer','minimum':0,'maximum':10000000},'temporary':BOOL,
    'verification':{'anyOf':[RUN_TO_VERIFICATION,{'type':'null'}]},'targetHit':{'type':['boolean','null']},
    'stopReason':{'type':'string','maxLength':512,'not':{'pattern':'\u0000'}},
    'message':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}},'continueResult':DEBUG_EXECUTION_RESULT},
    ('status','revision','active','restorePending','replyPending','stopConfirmed','sessionGeneration','path','line',
     'temporary','verification','targetHit','stopReason','message','continueResult'))
DEBUG_STATE_FIELDS = {'state':{'type':'string','enum':['idle','starting','running','stopped','exited','failed']},
    'reason':STRING,'error':STRING,'threads':ARRAY,'frames':ARRAY,'scopes':ARRAY,'variables':ARRAY,'output':STRING,
    'generation':{'type':'integer'},'threadId':{'type':'integer'},'frameId':{'type':'integer'},'terminalId':STRING,
    'breakpoints':{'type':'object','additionalProperties':BREAKPOINTS},'verifiedBreakpoints':{'type':'object'},'capabilities':{'type':'object'},
    'lastBreakpoint':{'type':'object'},'stopGeneration':{'type':'integer'},'exitCode':{'type':'integer'},'watches':ARRAY,'attached':BOOL,'runtime':{'type':'string','enum':['native','dotnet','adapter','java']},
    'adapterId':STRING,'adapterPlugin':STRING,'adapterTitle':STRING,'adapterRevoked':BOOL,
    'javaAttach':JAVA_ATTACH,
    'dataBreakpoints':{'type':'array','maxItems':32,'items':DATA_ACTIVE},'dataBreakpointRevision':NONNEGATIVE,'dataBreakpointsSynchronized':BOOL,
    'instructionBreakpoints':{'type':'array','maxItems':128,'items':INSTRUCTION_ACTIVE},
    'instructionBreakpointRevision':NONNEGATIVE,'instructionBreakpointsSynchronized':BOOL,
    'stepInTargetCatalog':{'type':'string','maxLength':96,'not':{'pattern':'\u0000'}},
    'gotoTargetCatalog':{'type':'string','maxLength':96,'not':{'pattern':'\u0000'}},'pendingStop':BOOL,
    'runToCursor':RUN_TO_STATE,
    'sessionGeneration':NONNEGATIVE,'lastMutation':DEBUG_MUTATION_STATE,'lastExecution':DEBUG_EXECUTION_STATE,
    'javaReload':schema({'status':{'type':'string','enum':['building','applying','applied','unchanged','failed','cancelled']},
        'changedClasses':{'type':'array','maxItems':2048,'items':{'type':'string','minLength':1,'maxLength':4096,'not':{'pattern':'\u0000'}}},
        'message':STRING,'operationId':{'type':'string','maxLength':128},'cancelRequested':BOOL},('status','changedClasses','message')),
    'javaReloadEvent':schema({'changeType':{'type':'string','enum':['ERROR','WARNING','STARTING','END','BUILD_COMPLETE']},
        'message':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}}},('changeType','message')),
    'javaReloadDiagnostic':schema({'changeType':{'type':'string','enum':['ERROR','WARNING']},
        'message':{'type':'string','maxLength':8192,'not':{'pattern':'\u0000'}}},('changeType','message')),
    'functionBreakpoints':ARRAY,'verifiedFunctionBreakpoints':ARRAY,'exceptionFilters':{'type':['array','null'],'items':STRING}}
DEBUG_STATE = schema(DEBUG_STATE_FIELDS,('state','reason','error','threads','frames','scopes','variables','output','generation','threadId','frameId','terminalId','breakpoints','verifiedBreakpoints','capabilities'))
FUNCTION_BREAKPOINTS={'type':'array','maxItems':128,'items':schema({'name':{'type':'string','minLength':1,'maxLength':4096},
    'enabled':BOOL,'condition':{'type':'string','maxLength':4096},'hitCondition':{'type':'string','maxLength':4096}},('name',))}
EXCEPTION_FILTERS={'type':['array','null'],'maxItems':64,'uniqueItems':True,'items':{'type':'string','minLength':1,'maxLength':128}}
DEBUG_INSPECTION=schema({'body':{'type':'object'},'stopGeneration':NONNEGATIVE},('body','stopGeneration'))
PACKAGE_REV = {'type':'string','pattern':'^(none|[a-f0-9]{128})$'}
PACKAGE_DIGEST = {'type':'string','pattern':'^[a-f0-9]{128}$'}
PACKAGE_ID = {'type':'string','pattern':'^[a-z][a-z0-9._-]{0,99}$'}
PACKAGE_PREVIEW = schema({'id':PACKAGE_ID,'name':STRING,'version':STRING,'platform':STRING,
    'digest':PACKAGE_DIGEST,'publicKey':{'type':'string','pattern':'^[a-f0-9]{64}$'},'fingerprint':PACKAGE_DIGEST,
    'permissions':{'type':'array','maxItems':32,'items':STRING},'dependencies':ARRAY,
    'fileCount':{'type':'integer','minimum':1,'maximum':256},'bytes':{'type':'integer','minimum':0,'maximum':268435456},
    'compatibility':schema({'compatible':BOOL,'reason':STRING,'reasons':{'type':'array','items':STRING}},('compatible','reason','reasons')),
    'revision':PACKAGE_REV,'trustRevision':PACKAGE_REV,'trusted':BOOL,'signatureValid':BOOL,'payloadVerified':BOOL,
    'source':STRING,'current':{'anyOf':[schema({'current':PACKAGE_DIGEST,'history':{'type':'array','maxItems':4,'items':PACKAGE_DIGEST}},('current','history')),{'type':'null'}]},'compatible':BOOL},
    ('id','name','version','platform','digest','publicKey','fingerprint','permissions','dependencies','fileCount','bytes','compatibility','revision','trustRevision','trusted','signatureValid','payloadVerified','source','current','compatible'))
PACKAGE_CATALOG = schema({'revision':PACKAGE_REV,'trustRevision':PACKAGE_REV,'entries':ARRAY,'publishers':ARRAY,
    'garbage':ARRAY,'garbageTruncated':BOOL,'storageErrors':ARRAY,'action':STRING,'id':PACKAGE_ID,'restartRequired':BOOL,'catalogError':STRING},
    ('revision','trustRevision','entries','publishers','garbage','garbageTruncated'))
GIT_GUARD = {'type':'string','minLength':1,'maxLength':256}
GIT_WORKTREE = schema({'path':STRING,'root':STRING,'head':STRING,'branch':STRING,'bare':BOOL,'detached':BOOL,'directoryIdentity':STRING,
    'locked':BOOL,'lockReason':STRING,'prunable':BOOL,'pruneReason':STRING,'primary':BOOL,
    'insideWorkspace':BOOL,'current':BOOL,'available':BOOL},
    ('path','root','head','branch','bare','detached','directoryIdentity','locked','lockReason','prunable','pruneReason','primary','insideWorkspace','current','available'))
GIT_WORKTREES = schema({'repository':PATH,'identity':GIT_GUARD,'revision':GIT_GUARD,
    'entries':{'type':'array','maxItems':256,'items':GIT_WORKTREE}},('repository','identity','revision','entries'))
GIT_WORKTREE_RESULT = schema({**PROCESS['properties'],'catalog':GIT_WORKTREES,'catalogError':STRING},PROCESS['required'])
GIT_WORKTREE_PATH = {'type':'string','minLength':1,'maxLength':4096,'not':{'pattern':'[\u0000-\u001f\u007f]'}}
GIT_WORKTREE_MAINTENANCE_FIELDS = {'path':GIT_WORKTREE_PATH,'destination':GIT_WORKTREE_PATH,
    'expectedRepository':GIT_GUARD,'expectedRevision':GIT_GUARD}
GIT_WORKTREE_MAINTENANCE_REQUEST = schema({'action':{'type':'string','enum':['move','repair']},
    'path':GIT_WORKTREE_PATH,'destination':GIT_WORKTREE_PATH},('action','path','destination'))
GIT_WORKTREE_PLAN = schema({'repository':PATH,'identity':GIT_GUARD,'catalogRevision':GIT_GUARD,'revision':GIT_GUARD,
    'request':GIT_WORKTREE_MAINTENANCE_REQUEST,'entry':GIT_WORKTREE,
    'arguments':{'type':'array','minItems':4,'maxItems':5,'items':{'type':'string','maxLength':32768}},
    'checkedRoots':{'type':'array','minItems':1,'maxItems':256,'items':GIT_WORKTREE_PATH},
    'movesDirectory':BOOL,'repairsLinks':BOOL,'preservesLocalFiles':{'const':True},
    'preservesIndex':{'const':True},'mayContactRemote':{'const':False}},
    ('repository','identity','catalogRevision','revision','request','entry','arguments','checkedRoots',
     'movesDirectory','repairsLinks','preservesLocalFiles','preservesIndex','mayContactRemote'))
GIT_SUBMODULE_PATH = {'type':'string','minLength':1,'maxLength':4096,'not':{'pattern':'\u0000'}}
GIT_OBJECT_OR_EMPTY = {'type':'string','pattern':'^(?:[0-9a-f]{40}|[0-9a-f]{64})?$','not':{'pattern':'[^0-9a-f]'}}
GIT_SUBMODULE = schema({'name':{'type':'string','minLength':1,'maxLength':512},'path':GIT_SUBMODULE_PATH,'root':PATH,
    'expectedCommit':GIT_OBJECT_OR_EMPTY,'head':GIT_OBJECT_OR_EMPTY,'branch':{'type':'string','maxLength':512},
    'registered':BOOL,'checkedOut':BOOL,'dirty':{'type':['boolean','null']},'directoryIdentity':STRING,'repositoryIdentity':STRING,
    'state':{'type':'string','enum':['notIndexed','conflicted','uninitialized','matching','different','unavailable']},
    'error':{'type':'string','maxLength':8192}},
    ('name','path','root','expectedCommit','head','branch','registered','checkedOut','dirty','directoryIdentity','repositoryIdentity','state','error'))
GIT_SUBMODULES = schema({'repository':PATH,'identity':GIT_GUARD,'revision':GIT_GUARD,
    'entries':{'type':'array','maxItems':64,'items':GIT_SUBMODULE}},('repository','identity','revision','entries'))
GIT_SUBMODULE_RESULT = schema({**PROCESS['properties'],'catalog':GIT_SUBMODULES,'catalogError':STRING},PROCESS['required'])
GIT_SUBMODULE_GUARD = {'path':GIT_SUBMODULE_PATH,'expectedRepository':GIT_GUARD,'expectedRevision':GIT_GUARD}
GIT_SUBMODULE_ADD_FIELDS = {**GIT_SUBMODULE_GUARD,
    'url':{'type':'string','minLength':1,'maxLength':4096,'not':{'pattern':'[\u0000-\u001f\u007f]'}},
    'name':{'type':'string','maxLength':512,'not':{'pattern':'[\u0000-\u001f\u007f]'}},
    'branch':{'type':'string','maxLength':256,'not':{'pattern':'[\u0000-\u001f\u007f]'}}}
GIT_SUBMODULE_ADD_REQUIRED = (*GIT_SUBMODULE_GUARD,'url')
GIT_SUBMODULE_ADD_REQUEST = schema({'url':GIT_SUBMODULE_ADD_FIELDS['url'],'path':GIT_SUBMODULE_PATH,
    'name':{**GIT_SUBMODULE_ADD_FIELDS['name'],'minLength':1},'branch':GIT_SUBMODULE_ADD_FIELDS['branch']},('url','path','name','branch'))
GIT_SUBMODULE_ADD_PLAN = schema({'repository':PATH,'identity':GIT_GUARD,'catalogRevision':GIT_GUARD,'revision':GIT_GUARD,
    'request':GIT_SUBMODULE_ADD_REQUEST,'root':PATH,
    'arguments':{'type':'array','minItems':7,'maxItems':9,'items':{'type':'string','maxLength':4096}},
    'createsCheckout':{'const':True},'stagesGitmodules':{'const':True},'stagesGitlink':{'const':True},
    'mayContactRemote':{'const':True},'remoteCommit':{'type':'null'}},
    ('repository','identity','catalogRevision','revision','request','root','arguments','createsCheckout','stagesGitmodules','stagesGitlink','mayContactRemote','remoteCommit'))
GIT_SUBMODULE_DEINIT_PLAN = schema({'repository':PATH,'identity':GIT_GUARD,'catalogRevision':GIT_GUARD,'revision':GIT_GUARD,
    'entry':GIT_SUBMODULE,'trackedFiles':{'type':'integer','minimum':0,'maximum':20000},
    'arguments':{'type':'array','minItems':5,'maxItems':5,'items':{'type':'string','maxLength':4096}},
    'removesCheckout':{'const':True},'unregistersLocally':{'const':True},'preservesGitlink':{'const':True},
    'preservesGitmodules':{'const':True},'preservesRepository':{'const':True},'mayContactRemote':{'const':False}},
    ('repository','identity','catalogRevision','revision','entry','trackedFiles','arguments','removesCheckout',
     'unregistersLocally','preservesGitlink','preservesGitmodules','preservesRepository','mayContactRemote'))
GIT_SUBMODULE_SETTING_FIELDS = {'path':GIT_SUBMODULE_PATH,'setting':{'type':'string','enum':['url','branch']},
    'value':{'type':'string','maxLength':4096,'not':{'pattern':'[\u0000-\u001f\u007f]'}}}
GIT_SUBMODULE_SETTING_REQUEST = {**schema(GIT_SUBMODULE_SETTING_FIELDS,tuple(GIT_SUBMODULE_SETTING_FIELDS)),
    'allOf':[{'if':{'properties':{'setting':{'const':'url'}}},'then':{'properties':{'value':{'minLength':1}}}},
             {'if':{'properties':{'setting':{'const':'branch'}}},'then':{'properties':{'value':{'maxLength':256}}}}]}
GIT_SUBMODULE_SETTING_INPUT = {**GIT_SUBMODULE_SETTING_REQUEST,
    'properties':{**GIT_SUBMODULE_SETTING_REQUEST['properties'],**GIT_SUBMODULE_GUARD},
    'required':list(dict.fromkeys((*GIT_SUBMODULE_SETTING_FIELDS,*GIT_SUBMODULE_GUARD)))}
GIT_SUBMODULE_SETTINGS_PLAN = schema({'repository':PATH,'identity':GIT_GUARD,'catalogRevision':GIT_GUARD,'revision':GIT_GUARD,
    'request':GIT_SUBMODULE_SETTING_REQUEST,'entry':GIT_SUBMODULE,
    'previousValue':{'anyOf':[GIT_SUBMODULE_SETTING_FIELDS['value'],{'type':'null'}]},
    'previousRedacted':BOOL,'hasConfiguredValue':BOOL,'hasLocalBranchOverride':BOOL,'alreadyConfigured':BOOL,
    'arguments':{'type':'array','minItems':5,'maxItems':6,'items':{'type':'string','maxLength':4096}},
    'writesGitmodules':BOOL,'maySynchronizeLocalUrl':BOOL,'stagesGitmodules':{'const':False},
    'changesCheckout':{'const':False},'mayContactRemote':{'const':False}},
    ('repository','identity','catalogRevision','revision','request','entry','previousValue','previousRedacted',
     'hasConfiguredValue','hasLocalBranchOverride','alreadyConfigured','arguments','writesGitmodules',
     'maySynchronizeLocalUrl','stagesGitmodules','changesCheckout','mayContactRemote'))
GIT_CONFLICT_PATH = {'type':'string','minLength':1,'maxLength':4096,
    'description':'Path relative to the selected Git repository, not the workspace. Index resolution accepts portable ordinary file paths only.'}
GIT_CONFLICT_VERSION = schema({'mode':{'type':'string','enum':['100644','100755','120000','160000','040000']},
    'oid':{'type':'string','pattern':'^(?:[0-9a-f]{40}|[0-9a-f]{64})$'}},('mode','oid'))
GIT_CONFLICT_NULLABLE_VERSION = {'anyOf':[GIT_CONFLICT_VERSION,{'type':'null'}]}
GIT_CONFLICT_ENTRY = schema({'path':GIT_CONFLICT_PATH,'documentPath':PATH,
    'status':{'type':'string','enum':['DD','AU','UD','UA','DU','AA','UU']},
    **{name:GIT_CONFLICT_NULLABLE_VERSION for name in ('base','ours','theirs')},'supported':BOOL,'reason':STRING},
    ('path','documentPath','status','base','ours','theirs','supported','reason'))
GIT_CONFLICT_CATALOG = schema({'repository':PATH,'identity':GIT_GUARD,'revision':GIT_GUARD,
    'entries':{'type':'array','maxItems':512,'items':GIT_CONFLICT_ENTRY}},('repository','identity','revision','entries'))
GIT_CONFLICT_CHOICES = {'type':'array','minItems':1,'maxItems':32,'items':schema({'path':GIT_CONFLICT_PATH,
    'choice':{'type':'string','enum':['ours','theirs','base','delete']}},('path','choice'))}
GIT_CONFLICT_CHANGES = {'type':'array','minItems':1,'maxItems':32,'items':schema({'path':GIT_CONFLICT_PATH,
    'choice':{'type':'string','enum':['ours','theirs','base','delete']},'version':GIT_CONFLICT_NULLABLE_VERSION,'deleted':BOOL},
    ('path','choice','version','deleted'))}
GIT_CONFLICT_CHANGES['items']['allOf'] = [
    {'if':{'properties':{'deleted':{'const':True}}},'then':{'properties':{'version':{'type':'null'}}},
     'else':{'properties':{'version':GIT_CONFLICT_VERSION}}}]
GIT_CONFLICT_PRESERVATION = {'preservesWorkingFiles':{'const':True},'preservesBuffers':{'const':True}}
GIT_CONFLICT_GUARDS = {'expectedRepository':GIT_GUARD,'expectedRevision':GIT_GUARD,'choices':GIT_CONFLICT_CHOICES}
GIT_CONFLICT_PLAN = schema({'repository':PATH,'identity':GIT_GUARD,'catalogRevision':GIT_GUARD,'revision':GIT_GUARD,
    'choices':GIT_CONFLICT_CHOICES,'changes':GIT_CONFLICT_CHANGES,
    'arguments':{'type':'array','minItems':4,'maxItems':164,'items':{'type':'string','maxLength':4096}},
    **GIT_CONFLICT_PRESERVATION,'mayContactRemote':{'const':False}},
    ('repository','identity','catalogRevision','revision','choices','changes','arguments','preservesWorkingFiles','preservesBuffers','mayContactRemote'))
GIT_CONFLICT_BLOB = schema({'kind':{'type':'string','enum':['absent','text','binary','large','unsupported']},
    'bytes':NONNEGATIVE,'text':{'type':['string','null'],'maxLength':4194304}},('kind','bytes','text'))
GIT_CONFLICT_BLOB['allOf'] = [
    {'if':{'properties':{'kind':{'const':'text'}}},'then':{'properties':{'text':{'type':'string'}}},
     'else':{'properties':{'text':{'type':'null'}}}},
    {'if':{'properties':{'kind':{'const':'absent'}}},'then':{'properties':{'bytes':{'const':0}}}}]
GIT_CONFLICT_RESULT = schema({**PROCESS['properties'],**GIT_CONFLICT_PRESERVATION,'changes':GIT_CONFLICT_CHANGES,
    'catalog':GIT_CONFLICT_CATALOG,'catalogError':STRING},(*PROCESS['required'],'preservesWorkingFiles','preservesBuffers','changes'))
GIT_CHECKOUT_ACTION = {'type':'string','enum':['materialize','remove']}
GIT_CHECKOUT_RECEIPT = {'type':'string','pattern':'^item-[0-9a-f]{32}$'}
GIT_CHECKOUT_PLAN = schema({'id':GIT_CHECKOUT_RECEIPT,'repository':PATH,'identity':GIT_GUARD,'indexRevision':GIT_GUARD,
    'revision':GIT_GUARD,'path':GIT_CONFLICT_PATH,'documentPath':PATH,'action':GIT_CHECKOUT_ACTION,
    'version':GIT_CONFLICT_NULLABLE_VERSION,'originalExisted':BOOL,'originalBytes':{'type':'integer','minimum':0,'maximum':16777216},
    'replacementBytes':{'type':'integer','minimum':0,'maximum':16777216},'blocked':BOOL,
    'preservesIndex':{'const':True},'backupsPersist':{'const':True},'rawBytes':{'const':True},'mayContactRemote':{'const':False}},
    ('id','repository','identity','indexRevision','revision','path','documentPath','action','version','originalExisted',
     'originalBytes','replacementBytes','blocked','preservesIndex','backupsPersist','rawBytes','mayContactRemote'))
GIT_CHECKOUT_PLAN['allOf']=[{'if':{'properties':{'action':{'const':'remove'}}},'then':{'properties':{'version':{'type':'null'},'replacementBytes':{'const':0}}},
    'else':{'properties':{'version':schema({'mode':{'type':'string','enum':['100644','100755']},'oid':GIT_CONFLICT_VERSION['properties']['oid']},('mode','oid'))}}}]
GIT_CHECKOUT_RESULT = schema({'id':GIT_CHECKOUT_RECEIPT,'repository':PATH,'identity':GIT_GUARD,'revision':GIT_GUARD,
    'path':GIT_CONFLICT_PATH,'documentPath':PATH,'state':{'type':'string','enum':['applied','undone','partial']},
    'backup':{'type':'string'},'undoBackup':{'type':'string'},'error':STRING,'inspectionError':STRING,
    'preservesIndex':{'const':True},'rawBytes':{'const':True},'canUndo':BOOL},
    ('id','repository','identity','revision','path','documentPath','state','backup','undoBackup','error','preservesIndex','rawBytes','canUndo'))
GIT_CHECKOUT_GUARDS={'id':GIT_CHECKOUT_RECEIPT,'expectedRepository':GIT_GUARD,'expectedPlanRevision':GIT_GUARD}
GIT_MERGE_OID={'type':'string','pattern':'^(?:[0-9a-f]{40}|[0-9a-f]{64})$','not':{'pattern':'[^0-9a-f]'}}
GIT_MERGE_BLOCKERS={'type':'array','maxItems':16,'uniqueItems':True,'items':{'type':'string','enum':[
    'notMerge','otherOperation','autostash','metadataOpen','headChanged','unmerged','workingChanges','dirtyBuffers','hiddenIndexFlags','multipleParents']}}
GIT_MERGE_PATHS={'type':'array','maxItems':2048,'items':schema({'status':{'type':'string','enum':['A','D','M','T','U']},'path':GIT_CONFLICT_PATH},('status','path'))}
GIT_MERGE_MESSAGE={'type':'string','maxLength':16384,'not':{'pattern':'\u0000'}}
GIT_MERGE_STATE=schema({'repository':PATH,'identity':GIT_GUARD,'revision':GIT_GUARD,'kind':{'type':'string','enum':['none','merge','other']},
    'head':{'anyOf':[GIT_MERGE_OID,{'const':''}]},'branch':STRING,'mergeHeads':{'type':'array','maxItems':16,'uniqueItems':True,'items':GIT_MERGE_OID},
    'message':GIT_MERGE_MESSAGE,'autostash':BOOL,'otherOperations':{'type':'array','maxItems':5,'items':{'type':'string','enum':['rebase-merge','rebase-apply','sequencer','cherry-pick','revert']}},
    'conflicts':{'type':'array','maxItems':2048,'items':GIT_CONFLICT_PATH},'stagedPaths':GIT_MERGE_PATHS,'stagedSummary':{'type':'string','maxLength':1048576},
    'workingDirty':BOOL,'hiddenIndexFlags':BOOL,'dirtyBuffers':NONNEGATIVE,'metadataDocuments':NONNEGATIVE,'openDocuments':NONNEGATIVE,
    'canCommit':BOOL,'canQuit':BOOL,'commitBlockers':GIT_MERGE_BLOCKERS,'quitBlockers':GIT_MERGE_BLOCKERS},
    ('repository','identity','revision','kind','head','branch','mergeHeads','message','autostash','otherOperations','conflicts','stagedPaths','stagedSummary',
     'workingDirty','hiddenIndexFlags','dirtyBuffers','metadataDocuments','openDocuments','canCommit','canQuit','commitBlockers','quitBlockers'))
GIT_MERGE_REQUEST={'action':{'type':'string','enum':['commit','quit']},'message':GIT_MERGE_MESSAGE,'runHooks':BOOL}
GIT_MERGE_REQUEST_SCHEMA=schema(GIT_MERGE_REQUEST,tuple(GIT_MERGE_REQUEST))
GIT_MERGE_REQUEST_SCHEMA['allOf']=[{'if':{'properties':{'action':{'const':'quit'}}},'then':{'properties':{'message':{'const':''},'runHooks':{'const':False}}}}]
GIT_MERGE_PLAN=schema({'repository':PATH,'identity':GIT_GUARD,'stateRevision':GIT_GUARD,'revision':GIT_GUARD,'request':GIT_MERGE_REQUEST_SCHEMA,
    'expectedParents':{'type':'array','maxItems':2,'items':GIT_MERGE_OID},'stagedPaths':GIT_MERGE_PATHS,'stagedSummary':{'type':'string','maxLength':1048576},
    'preservesWorkingFiles':BOOL,'preservesIndex':BOOL,'createsCommit':BOOL,'mayExecuteHooks':BOOL,'signsCommit':{'const':False},'mayContactRemote':BOOL,
    'argumentsTemplate':{'type':'array','minItems':2,'maxItems':5,'items':STRING}},
    ('repository','identity','stateRevision','revision','request','expectedParents','stagedPaths','stagedSummary','preservesWorkingFiles','preservesIndex','createsCommit','mayExecuteHooks','signsCommit','mayContactRemote','argumentsTemplate'))
GIT_MERGE_RESULT=schema({**PROCESS['properties'],'action':{'type':'string','enum':['commit','quit']},'receipt':PATH,
    'newCommit':{'anyOf':[GIT_MERGE_OID,{'const':''}]},'state':GIT_MERGE_STATE,'inspectionError':STRING},(*PROCESS['required'],'action','receipt','newCommit'))
GIT_MERGE_GUARDS={'expectedRepository':GIT_GUARD,'expectedRevision':GIT_GUARD}
GIT_MERGE_INPUT_RULES=[
    {'if':{'properties':{'action':{'const':'quit'}}},'then':{'properties':{'message':{'const':''},'runHooks':{'const':False}}},
     'else':{'required':['message'],'properties':{'message':{**GIT_MERGE_MESSAGE,'minLength':1,'pattern':'[^ \\t\\r\\n]'}}}}]
GIT_MERGE_REQUEST_SCHEMA['allOf']=GIT_MERGE_INPUT_RULES
GIT_REPLAY_ACTION={'type':'string','enum':['cherryPick','revert','continue','quit']}
GIT_REPLAY_REQUEST={'action':GIT_REPLAY_ACTION,'commit':{'anyOf':[GIT_MERGE_OID,{'const':''}]}}
GIT_REPLAY_RULES=[{'if':{'properties':{'action':{'enum':['cherryPick','revert']}}},
    'then':{'required':['commit'],'properties':{'commit':GIT_MERGE_OID}},
    'else':{'properties':{'commit':{'const':''}}}}]
GIT_REPLAY_REQUEST_SCHEMA=schema(GIT_REPLAY_REQUEST,tuple(GIT_REPLAY_REQUEST))
GIT_REPLAY_REQUEST_SCHEMA['allOf']=GIT_REPLAY_RULES
GIT_REPLAY_BLOCKERS={'type':'array','maxItems':12,'uniqueItems':True,'items':{'type':'string','enum':[
    'metadataOpen','autostash','activeOperation','notSingleReplay','unborn','openDocuments','workingChanges','hiddenIndexFlags','unmerged','stagedChanges']}}
GIT_REPLAY_STATE_PROPERTIES={key:value for key,value in GIT_MERGE_STATE['properties'].items() if key in (
    'repository','identity','revision','head','branch','otherOperations','conflicts','stagedPaths','stagedSummary',
    'workingDirty','hiddenIndexFlags','dirtyBuffers','metadataDocuments','openDocuments','autostash')}
GIT_REPLAY_STATE_PROPERTIES.update({'kind':{'type':'string','enum':['none','cherryPick','revert','other']},
    'operationCommit':{'anyOf':[GIT_MERGE_OID,{'const':''}]},'canStart':BOOL,'canContinue':BOOL,'canQuit':BOOL,
    'startBlockers':GIT_REPLAY_BLOCKERS,'continueBlockers':GIT_REPLAY_BLOCKERS,'quitBlockers':GIT_REPLAY_BLOCKERS})
GIT_REPLAY_STATE=schema(GIT_REPLAY_STATE_PROPERTIES,tuple(GIT_REPLAY_STATE_PROPERTIES))
GIT_REPLAY_PLAN_PROPERTIES={'repository':PATH,'identity':GIT_GUARD,'stateRevision':GIT_GUARD,'revision':GIT_GUARD,
    'request':GIT_REPLAY_REQUEST_SCHEMA,'sourceCommit':GIT_MERGE_OID,'sourceMessage':GIT_MERGE_MESSAGE,
    'sourceParents':{'type':'array','maxItems':1,'items':GIT_MERGE_OID},'paths':GIT_MERGE_PATHS,
    'summary':{'type':'string','maxLength':1048576},'expectedParent':GIT_MERGE_OID,
    'arguments':{'type':'array','minItems':2,'maxItems':4,'items':STRING},'createsCommit':BOOL,
    'preservesWorkingFiles':BOOL,'preservesIndex':BOOL,'mayExecuteHooks':{'const':False},
    'signsCommit':{'const':False},'mayExecuteProjectTools':BOOL}
GIT_REPLAY_PLAN=schema(GIT_REPLAY_PLAN_PROPERTIES,tuple(GIT_REPLAY_PLAN_PROPERTIES))
GIT_REPLAY_RESULT=schema({**PROCESS['properties'],'action':GIT_REPLAY_ACTION,'receipt':PATH,
    'newCommit':{'anyOf':[GIT_MERGE_OID,{'const':''}]},'state':GIT_REPLAY_STATE,'inspectionError':STRING},
    (*PROCESS['required'],'action','receipt','newCommit'))
ANALYSIS_COVERAGE = schema({key:NONNEGATIVE for key in ('diagnosticClean','diagnosticErrors','diagnosticUnknown',
    'commands','translationUnits','outOfScope','observed','remaining')},
    ('diagnosticClean','diagnosticErrors','diagnosticUnknown','commands','translationUnits','outOfScope','observed','remaining'))
ANALYSIS_DOCUMENT = schema({'path':PATH,'revision':REVISION,'diagnostics':{'enum':['clean','errors','unknown']},'errors':NONNEGATIVE},
    ('path','revision','diagnostics','errors'))
ANALYSIS_STATE = schema({'state':{'enum':['idle','running','pending','completed','failed','cancelled']},'phase':STRING,
    'completed':NONNEGATIVE,'total':NONNEGATIVE,'progress':{'type':'number','minimum':0,'maximum':100},'error':STRING,
    'session':STRING,'coverage':ANALYSIS_COVERAGE,'resumable':BOOL},('state','phase','completed','total','progress','error'))
ANALYSIS_RESULT = schema({**ANALYSIS_STATE['properties'],
    'documents':{'type':'array','maxItems':128,'items':ANALYSIS_DOCUMENT},
    'scope':{'enum':['explicit-files','workspace-spelling-candidates','compilation-database']},
    'wholeProjectIndexCompleteness':{'const':'unknown'},'continuation':STRING,'nextOffset':NONNEGATIVE},
    (*ANALYSIS_STATE['required'],'session','coverage','documents','scope','wholeProjectIndexCompleteness','continuation','nextOffset'))
ANALYSIS_INPUT = schema({'paths':{'type':'array','minItems':1,'uniqueItems':True,'items':PATH},
    'scope':{'const':'compilation-database'},'pathPrefix':PATH,'continuation':STRING,
    'batchSize':{'type':'integer','minimum':0,'maximum':4096}})
ANALYSIS_INPUT['oneOf']=[{'required':['paths'],'not':{'anyOf':[{'required':[key]} for key in ('scope','pathPrefix','continuation')]}},
    {'required':['scope'],'not':{'anyOf':[{'required':[key]} for key in ('paths','continuation')]}},
    {'required':['continuation'],'properties':{'continuation':{'type':'string','minLength':1}},
     'not':{'anyOf':[{'required':[key]} for key in ('scope','pathPrefix','paths')]}}]
ANALYSIS_PAGE = schema({'session':STRING,'documents':{'type':'array','maxItems':256,'items':ANALYSIS_DOCUMENT},
    'offset':NONNEGATIVE,'nextOffset':NONNEGATIVE,'available':NONNEGATIVE,'total':NONNEGATIVE,'hasMore':BOOL},
    ('session','documents','offset','nextOffset','available','total','hasMore'))
SERVICE_STATUS = schema({'protocol':{'const':'cycode-core/1'},'version':STRING,
    'service':schema({'state':{'enum':['ready','draining']},'uptimeMilliseconds':NONNEGATIVE},('state','uptimeMilliseconds')),
    'engine':schema({'context':STRING,'state':STRING,'workspace':STRING},('context','state','workspace')),
    'analysis':schema({'statusMethod':{'const':'language.status'},'projectIndexCompleteness':{'const':'unknown'},'readyMeaning':STRING},('statusMethod','projectIndexCompleteness','readyMeaning')),
    'limits':schema({'requestBytes':NONNEGATIVE,'responseBytes':NONNEGATIVE},('requestBytes','responseBytes'))},
    ('protocol','version','service','engine','analysis','limits'))

OPERATIONS = [
    ('service_status','service.status','Inspect the persistent native Core context, lifecycle and transport limits. Service ready is not proof of backend initialization or complete semantic indexing. Use language_status for per-backend state and analysis_status for semantic preparation progress.',schema(),SERVICE_STATUS,[]),
    ('analysis_status','analysis.status','Read aggregate native semantic-preparation progress without starting work. A completed preparation proves the stated source-file scope, not exhaustive whole-project semantic coverage. Per-job ownership, polling and cancellation remain CyToolsCore cy_get_job/cy_cancel_job.',schema(),ANALYSIS_STATE,[]),
    ('analysis_prepare','analysis.prepare','Prepare explicit existing sources without a fixed file-count ceiling, or enumerate the active C++ compilation database (optional workspace pathPrefix). Rolling compiler analysis retains one temporary native buffer at a time and preserves existing/dirty documents. Compilation-database scope defaults to batches of 64; batchSize 0 inventories without analysing. Continue pending/cancelled work with its continuation/session ID and optional batchSize; document, workspace or compiler-context changes invalidate the checkpoint. No restart-persistent proof. Returns first 128 metadata rows; analysis_results pages the rest. Version-bound clean/error/unknown diagnostics and observed/remaining units are distinct from exhaustive Unreal/index completeness. Slow provider attempts retry until cancellation or a real failure; no source saves or backend installation.',
     ANALYSIS_INPUT,ANALYSIS_RESULT,[]),
    ('analysis_results','analysis.results','Read a metadata page from the latest native preparation checkpoint. Rows are historical observations for that session, not proof of current whole-project completeness. Pending work is continued through analysis_prepare; SDK job ownership/cancellation is unchanged.',
     schema({'session':STRING,'offset':NONNEGATIVE,'limit':{'type':'integer','minimum':1,'maximum':256}},('session',)),ANALYSIS_PAGE,[]),
    ('unreal_automation_plan','unreal.automationPlan','Preview one Unreal Engine 5 cook/package action without executing tools or creating output files. Requires a discovered non-Editor game target and a precompiled AutomationTool/.NET host plus editor commandlet binary. Supports explicit existing /Game map packages, desktop platform/configuration, build, Pak/IoStore/compression and a 1..86400-second deadline (3600 default). Source descriptors/rules/map stamps, engine version/tool stamps, workspace/engine identities and exact argv are reviewed. Issues one of 16 native previews for 10 minutes, with a new managed staging/archive/log folder. Trusted Unreal/project tools can modify normal cook/build/DDC/engine caches; no deploy/run/sign/upload, SDK installation or arbitrary arguments. An argument preview is not a complete content snapshot or package validation.',
     schema(UNREAL_AUTOMATION_FIELDS,('action','target')),UNREAL_AUTOMATION_PLAN,[]),
    ('unreal_automation_run','unreal.automationRun','Execute a matching issued cook/package preview once, after revision/tool/input/output checks and saved/reconciled workspace documents. Requires the exact reviewed timeoutSeconds and expectedRevision. Uses precompiled AutomationTool directly with argv and the selected .NET host; executes trusted project rules/content tools. Owns the process tree through shared build.status/build.cancel and operationId; cancellation/timeouts retain generated work. Keeps persistent intent.json, bounded raw output.log, native result.json and UAT logs under the issued workspace directory, separate from SDK temporary artifacts. A newly staged/archive folder avoids overwriting a previous managed archive, but normal Unreal cook/build/DDC/engine caches can change. Actual process status, output repair, archive-file observation and inspection/receipt errors remain explicit; exit zero plus an archive file does not validate installation. No packaged-game launch, deployment, automatic retry, rollback, editor Undo or artifact deletion.',
     schema(UNREAL_AUTOMATION_RUN_FIELDS,tuple(UNREAL_AUTOMATION_RUN_FIELDS)),UNREAL_AUTOMATION_RESULT,['process.execute','workspace.write']),
    ('vcs_replay_state','vcs.replay.state','Inspect selected Git state for single-commit cherry-pick/revert, reusing bounded complete merge/index/flags/metadata/document inspection. Reports actual pending object, conflicts, staged paths and separate start/continue/quit blockers. Two common inspections guard marker reads. Rebase/multi-commit sequencers remain external. Read-only commands may invoke trusted configured Git filters; not an OS sandbox.',schema(),GIT_REPLAY_STATE,[]),
    ('vcs_replay_plan','vcs.replay.plan','Preview one cherryPick/revert using a full 40/64-character lowercase commit object ID, or continue/quit the active single-commit replay with an empty commit field. Reject ranges/options and merge commits needing mainline. Start requires clean tracked/index state and closed repository documents; continue also requires resolved synchronized files and normal index flags. Quit preserves HEAD/index/files, accepts unsaved source buffers and is distinct from abort/reset. Source paths/stat/message are a review, not a resulting-tree simulation. Hooks/signing/editor/transport/maintenance/rerere are disabled; trusted filters/merge drivers may run project tools. No mutation.',
     schema({**GIT_REPLAY_REQUEST,**GIT_MERGE_GUARDS},('action',*GIT_MERGE_GUARDS)),GIT_REPLAY_PLAN,[]),
    ('vcs_replay_apply','vcs.replay.apply','Apply the matching reviewed replay request with repository/state/plan guards, persistent intent receipt and pre-dispatch index/flag recheck. Starts one unsigned cherry-pick/revert, continues after reviewed conflict resolution, or quits tracking without reset. Git itself protects colliding untracked files; no simulation or bytewise workspace backup. Uses fixed argv and a 120-second native-owned process. Retains actual exit/timeout/output and fresh state or inspectionError. Success checks actual raw commit parent, tree/current index and synchronized tracked state; continuation also verifies the exact reviewed staged tree. Quit verifies retained HEAD/index/flags. Conflicts/errors/timeouts require explicit inspection; no automatic retry, rollback or editor Undo. Noncancellable ownership.',
     schema({**GIT_REPLAY_REQUEST,**GIT_MERGE_GUARDS,'expectedPlanRevision':GIT_GUARD},('action',*GIT_MERGE_GUARDS,'expectedPlanRevision')),GIT_REPLAY_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_merge_state','vcs.merge.state','Inspect current merge/other-operation markers, full staged index/flags, HEAD/branch/admin identity, dirty buffers and unstaged tracked changes. Returns actual conflict paths, staged paths/stat, message, explicit commit/quit blockers and a reviewed revision. No mutation. Autostash/other operations are detected; commit completion supports one incoming merge parent. Bounds: 8 MiB/100000 index records, 2048 paths, 1 MiB stat, bounded metadata and a 45-second inspection budget with 15-second subprocess timeouts.',schema(),GIT_MERGE_STATE,[]),
    ('vcs_merge_plan','vcs.merge.plan','Review commit or quit against observed repository/state guards. Commit requires resolved index, synchronized tracked files, saved buffers, ordinary index flags and one incoming parent; the entire index will be committed. runHooks defaults false; enabling hooks requires closed repository documents and permits trusted project tools/network. Commit is unsigned with a verbatim UTF-8 message up to 16 KiB. Quit leaves files/index as-is and clears merge bookkeeping, so later ordinary commits no longer have those merge parents. Autostash/other operations/open Git metadata block both actions. Not git merge --abort and no reset.',
     schema({**GIT_MERGE_REQUEST,**GIT_MERGE_GUARDS},('action',*GIT_MERGE_GUARDS)),GIT_MERGE_PLAN,[]),
    ('vcs_merge_commit','vcs.merge.commit','Complete the exact reviewed single-parent-incoming merge through Git commit --file --cleanup=verbatim --no-gpg-sign. Requires matching commit action/message/hooks and observed repository/state/plan revisions. Stores a persistent intent receipt before dispatch, rechecks state, disables automatic maintenance/rerere/editor and Git transport; configured hooks only run when explicitly reviewed. A 120-second native-owned process may leave uncertain outcomes. Postflight verifies operation removal, actual ordered parents, committed tree and index records/flags. inspectionError retains uncertainty even after exit zero. No automatic retry, reset, rollback or editor Undo; inspect the actual new commit. Noncancellable ownership.',
     schema({**GIT_MERGE_REQUEST,**GIT_MERGE_GUARDS,'expectedPlanRevision':GIT_GUARD},('action',*GIT_MERGE_GUARDS,'expectedPlanRevision')),GIT_MERGE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_merge_quit','vcs.merge.quit','Quit the exact reviewed active merge through Git merge --quit, preserving working files and index entries rather than restoring pre-merge content. Dirty source buffers and unresolved index entries may remain; autostash, other operations and open Git metadata block the action. Requires matching quit action, empty message, no hooks and repository/state/plan guards. Stores an intent receipt before dispatch; postflight verifies removed bookkeeping and retained HEAD/index records/flags. Not merge --abort, reset or resumable editor Undo. Errors/timeouts/inspectionError require actual-state inspection, never automatic retry. Noncancellable ownership.',
     schema({**GIT_MERGE_REQUEST,**GIT_MERGE_GUARDS,'expectedPlanRevision':GIT_GUARD},('action',*GIT_MERGE_GUARDS,'expectedPlanRevision')),GIT_MERGE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_checkout_plan','vcs.checkout.plan','Preview a single working-file action, without changing index/files. materialize copies exact local stage-zero regular blob bytes; remove requires an absent index entry and explicitly removes the untracked local file. Repository-relative path and observed identity required. Ordinary closed text documents, existing parent, no redirects/nested repository/protected metadata/active project entry. Bounds: 16 MiB staged/original file, 64 MiB retained native preview bytes, 16 receipts, 10-minute resource preview, 8 MiB/100000 index records. No filters, EOL conversion, hooks or transport. Returns full index/branch/repository and resource preview guards.',
     schema({'path':GIT_CONFLICT_PATH,'action':GIT_CHECKOUT_ACTION,'expectedRepository':GIT_GUARD},('path','action','expectedRepository')),GIT_CHECKOUT_PLAN,[]),
    ('vcs_checkout_apply','vcs.checkout.apply','Apply an issued working-file preview after repository/full-index/resource/documents rechecks. Original content moves to persistent project trash before exclusive installation/removal; new binary bytes are exclusive-created in a private backup entry first. Never modifies Git index, auto-retries, permanently deletes displaced data or applies Git filters/EOL conversion. Closed text documents required. Two filesystem moves are not globally atomic: partial outcomes retain journal/payloads and require inspection. POSIX executable bits follow the staged regular mode; Windows does not emulate POSIX mode. A receipt permits separate guarded working-file Undo; backups remain accessible after restart through project trash. Noncancellable native ownership.',
     schema(GIT_CHECKOUT_GUARDS,tuple(GIT_CHECKOUT_GUARDS)),GIT_CHECKOUT_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_checkout_undo','vcs.checkout.undo','Undo one completed native working-file receipt without changing Git index. Requires observed repository and issued plan revision, closed documents, unchanged installed/missing state and intact original backup. Displaced current content moves to persistent trash before restoration through the common resource owner. Never overwrites newer content. Multiple moves can leave a partial result; retain both backup references and inspect rather than automatically retry. Native receipts are bounded/in-memory; persistent trash remains recoverable after restart. Noncancellable native ownership.',
     schema(GIT_CHECKOUT_GUARDS,tuple(GIT_CHECKOUT_GUARDS)),GIT_CHECKOUT_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_conflict_list','vcs.conflict.list','Catalogue actual unresolved Git index stages, including modify/delete, add/add and multiple rename-related paths, without opening text documents. NUL-delimited mode/OID records, SHA-1/SHA-256, complete index/HEAD/branch/repository guard. No rename topology is inferred. Bounds: 8 MiB index listing, 100000 records, 512 conflicts, 45-second local inspection budget. No transport or configured hooks/fsmonitor requested.',schema(),GIT_CONFLICT_CATALOG,[]),
    ('vcs_conflict_inspect','vcs.conflict.inspect','Read one observed conflict version set safely. Reports absent/text/binary/large/unsupported, byte counts and nullable UTF-8 text, at most 4 MiB per version. Binary bytes never enter JSON/text buffers. Does not read or alter working files. All paths are repository-relative; catalogue revision required.',
     schema({'path':GIT_CONFLICT_PATH,'expectedRevision':GIT_GUARD},('path','expectedRevision')),
     schema({'catalogRevision':GIT_GUARD,'entry':GIT_CONFLICT_ENTRY,'versions':schema({name:GIT_CONFLICT_BLOB for name in ('base','ours','theirs')},('base','ours','theirs'))},('catalogRevision','entry','versions')),[]),
    ('vcs_conflict_plan','vcs.conflict.plan','Preview one to 32 explicit index resolutions from observed stages. A missing chosen side or explicit delete removes only the index entry. Keeps working files and live buffers intact, without text conversion. Review all rename paths explicitly; no automatic pairing, worktree rename or checkout. Rejects symlink/gitlink conflicts, ambiguous/overlapping paths and argv beyond 12000 path bytes. Requires observed repository/catalogue guards.',
     schema(GIT_CONFLICT_GUARDS,tuple(GIT_CONFLICT_GUARDS)),GIT_CONFLICT_PLAN,[]),
    ('vcs_conflict_apply','vcs.conflict.apply','Apply exactly the reviewed index resolution in one native-owned Git update-index invocation. Chosen local blob OIDs and modes are staged directly, deletions remove index entries. Working files/open buffers stay unchanged and may still contain conflict markers. No filters, checkout, hook, remote or commit requested; Git owns index locking. Rechecks repository/catalogue/plan, verifies all selected and unselected mode/OID/stage records afterward. External-writer preflight races remain possible; errors/timeouts can leave an uncertain index. Inspect catalogError and refresh, never automatic retry/rollback. No editor Undo. Noncancellable while native owns the process.',
     schema({**GIT_CONFLICT_GUARDS,'expectedPlanRevision':GIT_GUARD},(*GIT_CONFLICT_GUARDS,'expectedPlanRevision')),GIT_CONFLICT_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_worktree_plan','vcs.worktree.plan','Preview moving or repairing one linked worktree inside the workspace. Requires observed repository/catalogue guards and action/path/destination. Move requires a new destination with an existing ordinary parent, no overlapping registered checkout, an unlocked non-primary/non-selected source and closed source/destination native documents. Local/staged/untracked/ignored payload is retained; no force or checkout. Repair reconnects an existing manually-moved folder to a missing registered path, only from an ordinary main checkout with every linked checkout inside this workspace, available, unlocked, reciprocal and closed. Audits all common worktree metadata to reject unreviewed/orphan records because Git repair may examine every linked worktree. Nested submodules/custom pointer layouts are refused. Returns fixed argv, checked paths, preservation effects and a separate revision binding metadata/index/HEAD/directories/pointers. No Git transport or mutation; 2 MiB query output, 200000 mode records, 64 MiB affected index, 256 directory levels and observed 45-second local inspection budget.',
     schema({**GIT_WORKTREE_MAINTENANCE_FIELDS,'action':{'type':'string','enum':['move','repair']}},(*GIT_WORKTREE_MAINTENANCE_FIELDS,'action')),GIT_WORKTREE_PLAN,[]),
    ('vcs_worktree_move','vcs.worktree.move','Move one reviewed linked checkout with fixed Git worktree move -- source destination and no force, transport, staging or commit. Requires the same path/destination and repository/catalogue guards plus expectedPlanRevision from vcs_worktree_plan(action=move). Revalidates ordinary directory/pointer identities and closed native documents before dispatch. Git renames the payload, preserving staged/local/untracked/ignored files; locked, nested-submodule, main/selected/overlapping or occupied destinations are refused. Does not copy across filesystems, delete partial results, automatically retry/rollback or create editor Undo. Postflight checks actual destination identity, retained HEAD/index, reciprocal links and catalogue. Returns actual process outcome and catalogue/catalogError, including exit-zero uncertainty. Noncancellable owner, 180-second process bound; no global external-writer transaction.',
     schema({**GIT_WORKTREE_MAINTENANCE_FIELDS,'expectedPlanRevision':GIT_GUARD},(*GIT_WORKTREE_MAINTENANCE_FIELDS,'expectedPlanRevision')),GIT_WORKTREE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_worktree_repair','vcs.worktree.repair','Repair one reviewed manually-moved linked checkout with fixed Git worktree repair -- destination. Requires the same path/destination and observed repository/catalogue guards plus expectedPlanRevision from vcs_worktree_plan(action=repair). Source must remain missing; destination must carry a valid pointer into this parent common worktree metadata and its old reciprocal backlink. Git may update linking files in all listed checkedRoots, so every registered checkout/common metadata record is audited, ordinary and inside this workspace; other missing/external/locked/orphan links and open linked native documents block repair. Uses absolute linking policy, without force, transport, checkout, staging, commit or editor Undo. Postflight checks actual source absence, destination identity, index/HEAD and reviewed reciprocal links/catalogue. Partial/uncertain results remain visible without retry/rollback. Noncancellable owner, 180-second process limit; not a global filesystem transaction.',
     schema({**GIT_WORKTREE_MAINTENANCE_FIELDS,'expectedPlanRevision':GIT_GUARD},(*GIT_WORKTREE_MAINTENANCE_FIELDS,'expectedPlanRevision')),GIT_WORKTREE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_submodule_settings_plan','vcs.submodule.settingsPlan','Preview changing one configured Git submodule URL or remote tracking branch. Requires observed repository/catalogue guards and path/setting/value. URL addresses share the add validator: no embedded credentials, helper commands, query tokens or fragments. Branch is a literal name, empty for remote HEAD or dot for the parent branch name; local branch overrides are reported and preserved. Native .gitmodules documents must be closed; URL changes also require closed child documents and ordinary parent-owned absorbed Git metadata for populated children. Normally tracked stage-zero .gitmodules is required without hidden flags/conflicts; existing staged and working-tree metadata changes are retained and bound by the preview revision. No mutation, staging, checkout or Git transport. Previous unsafe values are redacted; an already-configured branch is reported without inventing an executed process. Returns exact request/argv/effects, actual entry and separate observed preview revision. URL sync follows Git active-submodule semantics.',
     GIT_SUBMODULE_SETTING_INPUT,GIT_SUBMODULE_SETTINGS_PLAN,[]),
    ('vcs_submodule_settings','vcs.submodule.settings','Apply one reviewed URL/tracking-branch metadata change with the same inputs and expectedPlanRevision from vcs_submodule_settings_plan. Rechecks catalogue, .gitmodules/index, parent HEAD/reference, directories, Git pointer and closed documents. One fixed Git set-url --quiet or set-branch --branch/--default action, with no transport, force, staging, commit, checkout or editor Undo. URL may also synchronize local Git config; branch changes leave local overrides unchanged. Already-configured branches require no action and are refused before dispatch. Postflight verifies the requested value, preserved unrelated configuration records/index and stable checkout identity; actual process outcomes and catalogError retain partial/uncertain results. Noncancellable native owner, 180-second process limit, no automatic retry/rollback and no external-writer transaction.',
     {**GIT_SUBMODULE_SETTING_INPUT,'properties':{**GIT_SUBMODULE_SETTING_INPUT['properties'],'expectedPlanRevision':GIT_GUARD},
      'required':[*GIT_SUBMODULE_SETTING_INPUT['required'],'expectedPlanRevision']},GIT_SUBMODULE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_submodule_deinit_plan','vcs.submodule.deinitPlan','Preview deinitializing one registered absorbed Git submodule at its recorded commit. Requires observed repository/catalogue guards, closed native documents and normally tracked unchanged .gitmodules. Refuses modified, untracked or ignored files, hidden index flags, conflicts, nested gitlinks, redirected metadata and independent/embedded Git directories. Inspects at most 20000 child index records with 2 MiB command output and a 30-second preflight. No removal, staging or transport. Returns the exact selected entry, fixed quiet/no-force argv, tracked file count, removal/preservation effects and a separate revision binding the observed index, pointer, parent directories and metadata. Does not lock out external processes.',
     schema(GIT_SUBMODULE_GUARD,tuple(GIT_SUBMODULE_GUARD)),GIT_SUBMODULE_DEINIT_PLAN,[]),
    ('vcs_submodule_deinit','vcs.submodule.deinit','Remove one reviewed submodule checkout and its local configuration through Git submodule deinit --quiet -- path, without force/all/recursion or Git transports. Requires expectedPlanRevision from vcs_submodule_deinit_plan and the same repository/catalogue/path guards. Rechecks cleanliness including ignored files, normal index flags, closed native documents and retained parent-owned metadata immediately before dispatch. Preserves .gitmodules, the parent gitlink and absorbed repository; does not commit, stage, delete Git history or provide source Undo. Noncancellable native ownership, 180-second process limit, no automatic rollback/retry. Returns actual process status and a fresh catalogue or catalogError; verifies observed unregistration/checkout removal rather than trusting exit zero. Partial outcomes must be inspected. Not an atomic transaction against external writers.',
     schema({**GIT_SUBMODULE_GUARD,'expectedPlanRevision':GIT_GUARD},(*GIT_SUBMODULE_GUARD,'expectedPlanRevision')),GIT_SUBMODULE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_submodule_add_plan','vcs.submodule.addPlan','Preview adding one new Git submodule without clone/fetch or staging. Requires the observed repository/catalogue guards and url/path; optional name defaults to the path (at most 512 bytes), branch defaults to remote HEAD. Validates a new ordinary workspace destination with an existing parent in the same repository, clean tracked-or-absent .gitmodules, no open .gitmodules/destination documents, unoccupied metadata/config names and a valid literal branch. No overwrite, ignored-path force, existing-repository adoption or orphan metadata reactivation. Rejects embedded URL credentials/query tokens/custom helper command addresses; use Git credentials/SSH. Returns fixed argv/effects, unresolved remoteCommit:null and a revision binding normalized inputs and observed destinations/metadata. Does not contact the remote to resolve a commit. A preview is not a guarantee against external writers or remote changes.',
     schema(GIT_SUBMODULE_ADD_FIELDS,GIT_SUBMODULE_ADD_REQUIRED),GIT_SUBMODULE_ADD_PLAN,[]),
    ('vcs_submodule_add','vcs.submodule.add','Clone and stage one reviewed new Git submodule. Requires the same url/path/name/branch and repository/catalogue guards as the preview plus expectedPlanRevision from vcs_submodule_add_plan. Revalidates configuration/index/destination/metadata identities and closed documents before one Git submodule add. Git resolves the remote commit during cloning, writes/stages .gitmodules and its gitlink; unrelated staged files remain staged. No commit, recursive init, force, existing checkout/metadata reuse or automatic rollback/retry. Uses ordinary Git transport/credentials/hooks, a 180-second process deadline and retained native ownership (noncancellable). Failure can leave a partial clone/config/index; refresh and inspect before another reviewed action. Returns actual process status plus a fresh catalogue or catalogError. Not source-undoable or an atomic transaction against external processes.',
     schema({**GIT_SUBMODULE_ADD_FIELDS,'expectedPlanRevision':GIT_GUARD},(*GIT_SUBMODULE_ADD_REQUIRED,'expectedPlanRevision')),GIT_SUBMODULE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_submodules','vcs.submodules','Inspect configured top-level Git submodules in the selected workspace repository. Joins bounded .gitmodules config -z path records with staged gitlinks, actual child HEAD/branch/dirty state and registration. Reports conflicts, unavailable and not-indexed entries explicitly. No recursive discovery or clone/fetch. Raw URL/config values are private; the revision fingerprints configuration, index and observed child state. Maximum 64 modules, 1 MiB .gitmodules, 2 MiB command output and 30 seconds per complete catalogue. Noncancellable while the native inspection owns its processes.',
     schema(),GIT_SUBMODULES,[]),
    ('vcs_submodule_init','vcs.submodule.init','Register one configured indexed submodule URL locally using Git init, without downloading or checking out files. Requires observed repository identity and complete catalogue revision. Validates ordinary workspace paths and reconciled .gitmodules. Does not overwrite .gitmodules or automatically update the checkout. Refresh after failure; partial Git configuration remains. Noncancellable after submission.',
     schema(GIT_SUBMODULE_GUARD,tuple(GIT_SUBMODULE_GUARD)),GIT_SUBMODULE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_submodule_sync','vcs.submodule.sync','Synchronize one registered submodule URL from .gitmodules using Git sync. Requires observed repository/catalogue guards and a valid indexed ordinary path. Changes local Git configuration only, without checkout or download. Refresh after failure; no rollback/retry. Noncancellable after submission.',
     schema(GIT_SUBMODULE_GUARD,tuple(GIT_SUBMODULE_GUARD)),GIT_SUBMODULE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_submodule_update','vcs.submodule.update','Update one configured indexed submodule to the parent index commit via Git update --init --checkout --jobs=1, without force, recursion, remote-tip selection, merge, rebase or custom update commands. Requires observed repository/catalogue guards, clean child working tree and no open child documents at preflight. Checkout detaches HEAD and can execute trusted configured hooks. fetch=false (default) requires an existing checkout, passes --no-fetch and denies Git transport protocols; fetch=true permits clone/fetch through normal Git protocol policy. This is not an OS sandbox for hooks. No source Undo/automatic retry; failures/timeouts can leave a partial checkout/configuration. Results retain actual process status and a fresh catalogue or catalogError. Noncancellable after submission.',
     schema({**GIT_SUBMODULE_GUARD,'fetch':BOOL},tuple(GIT_SUBMODULE_GUARD)),GIT_SUBMODULE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('build_preset_plan','build.presetPlan','Inspect a test/package/workflow preset without executing CMake, CTest, CPack or project scripts. Resolves inherited local metadata and workflow steps, linked configure/binary roots and known output paths. Returns a revision-guarded argv plan; conditions and remaining execution semantics belong to the real tools.',
     schema(PRESET_ACTION_FIELDS,('kind','preset')),PRESET_PLAN,[]),
    ('build_preset_run','build.presetRun','Execute the reviewed CTest/CPack/CMake workflow preset through the existing native build owner. Requires expectedRevision from build_preset_plan and saved workspace documents. A standalone test/package requires a matching configured cache; workflow starts with configure. Uses argv, bounded output and owned process-tree cancellation. Executes trusted project/test/package code; never force-cleans or runs a fresh workflow. Partial outputs remain after failure/cancellation. inspectionError reports post-execution metadata failure separately; the completed command cannot be rolled back. The ordinary saved build profile is preserved.',
     schema({**PRESET_ACTION_FIELDS,'expectedRevision':{'type':'string','minLength':1,'maxLength':256}},('kind','preset','expectedRevision')),PRESET_RESULT,['process.execute']),
    ('language_hierarchy_prepare','language.hierarchyPrepare','Prepare an actual LSP call/type hierarchy at a workspace document UTF-16 position and current revision. Capability-gated; never enables a disabled backend. Returns issued IDs, not raw server data. Workspace locations and optional registered read-only SDK sources only. Bounded snapshots: five minutes, eight sessions/8 MiB, 512 nodes/2 MiB per session; refresh after edits, filesystem notifications or backend changes.',
     schema({'path':PATH,'position':POSITION,'expectedRevision':REVISION,'kind':HIERARCHY_KIND,'includeExternal':BOOL},('path','position','expectedRevision','kind')),HIERARCHY_RESULT,[]),
    ('language_hierarchy_expand','language.hierarchyExpand','Expand an issued hierarchy item using its original server and opaque data. Call directions are incoming/outgoing; type directions are supertypes/subtypes. Rejects stale/unknown items and contexts, with at most 256 returned relationships and 256 call sites per edge. Incoming sites are in the caller; outgoing sites are in the expanded source function. No caller-supplied URI/data or server command is accepted.',
     schema({'sessionId':HIERARCHY_ID,'item':HIERARCHY_ID,'direction':{'type':'string','enum':['incoming','outgoing','supertypes','subtypes']}},('sessionId','item','direction')),HIERARCHY_RESULT,[]),
    ('language_hierarchy_status','language.hierarchyStatus','Check whether a cached hierarchy context is still valid without sending a language-server request. Checks document identity/revision, native document events, workspace notification generation, backend epoch and expiry. Unknown/expired sessions return valid:false. This is not a synchronous filesystem rescan or proof that an asynchronous server index is complete.',
     schema({'sessionId':HIERARCHY_ID},('sessionId',)),schema({'valid':BOOL,'reason':STRING},('valid','reason')),[]),
    ('vcs_repositories','vcs.repositories','Discover ordinary .git directories and linked/submodule pointer files inside this workspace without executing Git or loading provider callbacks. Bounds: 20,000 directories, 200,000 inspected entries, depth 32, 256 roots; truncated/errors remain explicit. Automatic context lookup does not depend on this catalogue.',
     schema(),schema({'repositories':{'type':'array','maxItems':256,'items':schema({'root':PATH,'identity':GIT_GUARD,'linked':BOOL},('root','identity','linked'))},'truncated':BOOL,'visitedDirectories':NONNEGATIVE,'errors':ARRAY},('repositories','truncated','visitedDirectories','errors')),[]),
    ('vcs_worktrees','vcs.worktrees','Read the selected repository worktree list via complete porcelain -z records. External paths are catalogue-only; their directory availability is not probed. The returned revision guards worktree changes.',
     schema(),GIT_WORKTREES,[]),
    ('vcs_worktree_add','vcs.worktree.add','Create a linked checkout in a NEW workspace directory with an existing parent. newBranch uses a fresh local branch, existingBranch checks out an existing local branch, detached selects a verified commit. Refuses metadata destinations and open destination documents; never forces or overwrites. Git checkout can execute configured hooks. After a failure refresh; partial checkouts are retained.',
     schema({'destination':PATH,'mode':{'type':'string','enum':['newBranch','existingBranch','detached']},'branch':{'type':'string','maxLength':256},'start':{'type':'string','maxLength':512},'expectedRevision':GIT_GUARD},('destination','mode','expectedRevision')),GIT_WORKTREE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_worktree_remove','vcs.worktree.remove','Remove one reviewed, available linked worktree INSIDE the workspace. Refuses the primary, selected and workspace root, metadata paths, locks and open target documents. Uses Git remove without force, so tracked modifications, untracked files and submodules are refused. The branch remains. Refresh after transport/OS failure.',
     schema({'path':PATH,'expectedRevision':GIT_GUARD},('path','expectedRevision')),GIT_WORKTREE_RESULT,['vcs.write','workspace.write','process.execute']),
    ('vcs_worktree_lock','vcs.worktree.lock','Lock another available linked worktree inside the workspace against Git pruning/removal. Refuses primary/current/external roots. This is Git worktree administration, not a source-file lock.',
     schema({'path':PATH,'expectedRevision':GIT_GUARD,'reason':{'type':'string','maxLength':512}},('path','expectedRevision')),GIT_WORKTREE_RESULT,['vcs.write']),
    ('vcs_worktree_unlock','vcs.worktree.unlock','Explicitly remove the Git worktree administrative lock from another available linked worktree inside the workspace. Requires the observed catalogue revision.',
     schema({'path':PATH,'expectedRevision':GIT_GUARD},('path','expectedRevision')),GIT_WORKTREE_RESULT,['vcs.write']),
    ('packages_list','packages.list','Inspect installation-owned managed plugin selections, publisher trust and unused versions. Metadata/signatures only; no plugin code loaded. Existing live hosts keep their pinned versions.',schema(),PACKAGE_CATALOG,['plugins.execute']),
    ('packages_inspect','packages.inspect','Verify a signed local plugin package directory, payload hashes, platform and permissions without installing/loading code. Source must remain inside this workspace. Publisher enrollment is available only through explicit desktop interaction.',schema({'source':PATH},('source',)),PACKAGE_PREVIEW,['plugins.execute']),
    ('packages_install','packages.install','Install or update a reviewed signed package from a workspace directory. Requires an already trusted publisher and unchanged selection/trust/package digests. Versions are immutable; selection applies at next host startup. Does not load or replace running native code.',schema({'source':PATH,'expectedRevision':PACKAGE_REV,'expectedTrustRevision':PACKAGE_REV,'expectedPackage':PACKAGE_DIGEST},('source','expectedRevision','expectedTrustRevision','expectedPackage')),PACKAGE_CATALOG,['plugins.execute','process.execute','workspace.write']),
    ('packages_remove','packages.remove','Remove a managed plugin from the next-start selection after dependency validation. Running instances retain their pinned code; unused files use the separate guarded cleanup operation. Bundled/legacy plugins are not modified.',schema({'id':PACKAGE_ID,'expectedRevision':PACKAGE_REV},('id','expectedRevision')),PACKAGE_CATALOG,['plugins.execute','process.execute','workspace.write']),
    ('packages_rollback','packages.rollback','Select one retained signed version after trust, payload, platform and dependency checks. Requires unchanged selection/trust. Does not unload code from live instances.',schema({'id':PACKAGE_ID,'digest':PACKAGE_DIGEST,'expectedRevision':PACKAGE_REV,'expectedTrustRevision':PACKAGE_REV},('id','digest','expectedRevision','expectedTrustRevision')),PACKAGE_CATALOG,['plugins.execute','process.execute','workspace.write']),
    ('packages_clean','packages.clean','Delete one verified unused managed package using an exclusive lease and exact known file paths, without recursive deletion. Selected/rollback-retained or live-instance packages are refused; action inUse preserves the package.',schema({'id':PACKAGE_ID,'digest':PACKAGE_DIGEST,'expectedRevision':PACKAGE_REV},('id','digest','expectedRevision')),PACKAGE_CATALOG,['plugins.execute','process.execute','workspace.write']),
    ('events_read','events.read','Read bounded native workspace/document/resource/build event metadata without running plugin callbacks. Start with tail=true, take document/workspace snapshots, then resume with epoch and afterSequence=nextSequence. A gap requires resynchronization. Filters are exact types or trailing-star prefixes. No document text is included; use guarded native reads. Read-only identities may observe; native plugin host exports the same API.',
     schema({'epoch':{'type':'string','maxLength':64},'afterSequence':{'type':'integer','minimum':0,'maximum':9007199254740991},'limit':{'type':'integer','minimum':1,'maximum':512},'tail':BOOL,'excludeOrigin':{'type':'string','maxLength':160},'types':{'type':'array','maxItems':16,'items':{'type':'string','minLength':1,'maxLength':64}}}),
     schema({'epoch':STRING,'oldestSequence':NONNEGATIVE,'latestSequence':NONNEGATIVE,'nextSequence':NONNEGATIVE,'gap':BOOL,'hasMore':BOOL,'events':{'type':'array','maxItems':512,'items':schema({'sequence':REVISION,'type':STRING,'origin':STRING,'timestampMs':NONNEGATIVE,'data':{'type':'object'}},('sequence','type','origin','timestampMs','data'))}},('epoch','oldestSequence','latestSequence','nextSequence','gap','hasMore','events')),[]),
    ('tests_discover','tests.discover','Discover configured CTest tests through the actual CTest JSON model. Executes trusted project test definitions; does not compile or run the discovered tests. Issues one current catalogue per workspace.',schema({'buildDirectory':PATH,'configuration':STRING}),
     schema({'id':STRING,'buildDirectory':STRING,'configuration':STRING,'tests':ARRAY},('id','buildDirectory','configuration','tests')),['process.execute']),
    ('tests_run','tests.run','Run 1..128 selected issued CTest indices serially with per-test output/results and native process-tree cancellation. Refuses changed definitions and unsaved documents. Fixture dependencies may also execute. Timeout applies per selected test.',schema({'id':STRING,'tests':{'type':'array','minItems':1,'maxItems':128,'items':NONNEGATIVE},'timeoutSeconds':{'type':'integer','minimum':1,'maximum':120}},('id','tests')),
     schema({'id':STRING,'results':ARRAY,'cancelled':BOOL,'timedOut':BOOL,'truncated':BOOL,'exitCode':{'type':'integer'}},('id','results','cancelled','timedOut','truncated','exitCode')),['process.execute']),
    ('tests_cancel','tests.cancel','Request termination of the currently owned CTest process tree. The runner keeps its reservation until native execution has stopped.',schema({'operationId':STRING}),schema({'requested':BOOL},('requested',)),['process.execute']),
    ('templates_list','templates.list','List built-in and active plugin project templates, typed option fields and supported .NET target frameworks. Does not invoke a template callback, execute tools or install dependencies.',schema(),
     schema({'templates':{'type':'array','items':TEMPLATE_INFO},'frameworks':{'type':'array','items':STRING},'revision':NONNEGATIVE},('templates','frameworks','revision')),[]),
    ('templates_preview','templates.preview','Preview exact project files without filesystem writes. Destination is a new workspace-relative folder; its parent must exist. C/C++ CMake, Rust Cargo, C# .sln/.NET, standalone Java and dependency-free web templates. Plans expire after ten minutes; newest 32 retained.',
     schema({'template':STRING,'name':{'type':'string','minLength':1,'maxLength':64},'destination':PATH,'framework':STRING,'options':TEMPLATE_OPTIONS},('template','name')),TEMPLATE_PLAN,[]),
    ('templates_create','templates.create','Create exactly an issued project preview in a new folder under this workspace. Stages and verifies UTF-8 files, then exclusively renames without overwriting. Refuses changed parent directories. No process, dependency installation, Git initialization or license selection.',schema({'id':STRING},('id',)),TEMPLATE_PLAN,['workspace.write']),
    ('resources_preview','resources.preview','Prepare a bounded filesystem action without mutating project files. Paths are workspace-relative. Refuses links/protected metadata, existing destinations and the active project entry. Dirty targets are reported as blocked. This is a physical operation, not a semantic rename.',
     schema({'operation':{'type':'string','enum':['createFile','createDirectory','move','delete','restore']},'path':PATH,'destination':PATH,'trash':STRING},('operation',)),RESOURCE_PLAN,[]),
    ('resources_apply','resources.apply','Apply an issued filesystem plan after revision/content guards. Requires saved affected buffers. Deletes move to persistent project trash; no permanent deletion or destination overwrite.',schema({'id':STRING},('id',)),RESOURCE_PLAN,['workspace.write']),
    ('resources_undo','resources.undo','Undo an applied in-process filesystem action only while its content still matches and affected buffers are saved. Persistent deleted resources can also be restored through a new restore preview.',schema({'id':STRING},('id',)),RESOURCE_PLAN,['workspace.write']),
    ('resources_trash','resources.trash','List bounded persistent project-trash entries; restoring needs preview/apply and workspace.write. Original source files are not SDK task artifacts.',schema({'limit':{'type':'integer','minimum':1,'maximum':256}}),schema({'items':ARRAY,'errors':ARRAY,'truncated':BOOL},('items','errors','truncated')),[]),
    ('resources_children','resources.children','List one physical workspace directory, including empty folders and hidden/ignored project files. Protected metadata and links are excluded. Nonrecursive, bounded to 2048 results / 8192 examined entries; not the ripgrep search index.',schema({'path':PATH}),schema({'path':STRING,'items':ARRAY,'errors':ARRAY,'truncated':BOOL},('path','items','errors','truncated')),[]),
    ('resources_events','resources.events','Read bounded filesystem-operation events after a cursor. A gap requires document identity reconciliation; documents retain stable identities across moves.',schema({'after':{'type':'integer','minimum':0}}),schema({'revision':{'type':'integer','minimum':0},'events':ARRAY,'gap':BOOL},('revision','events','gap')),[]),
    ('project_inspect','project.inspect','Inspect a Cargo, npm, Java, Maven, Gradle or shader project and saved profile without executing project code, resolving dependencies or invoking compilers. Shader sources and installed tool paths are listed; no GPU preview.',schema(PROJECT_FIELDS),PROJECT_MODEL,[]),
    ('project_status','project.status','Inspect the currently owned Cargo/npm/Java/Maven/Gradle/shader task without waiting for execution, plugin or document locks. running remains true until task work returns; cancellation is a request. PTY applications have a separate terminal lifecycle.',schema(),TASK_STATUS,[]),
    ('project_output','project.output','Read the latest project task live display tail without waiting for execution locks. Same 256 KiB bounded journal, distinct stream IDs and conditional replacement semantics as build_output; supply both streamId/revision or neither. context identifies the task system/directory once validated; it can be empty before discovery and is absent before the first task. Cargo metadata is excluded; compiler-message rendered text is displayed and protocol artifacts are suppressed. Cargo framing caps each live protocol line at 64 KiB and reports an omission explicitly; the original process/final parser bytes stay unchanged. byteCount/droppedBytes count published display bytes, not original Cargo JSON bytes. Plain compiler/script text is merged stdout/stderr with UTF-8/control repair. Terminal applications, arbitrary plugin processes, retained history, push notifications and inferred progress are excluded. Final process outcomes/diagnostics remain on the original task result. Requires process.execute because logs may be private. Poll about 250 ms or slower.',BUILD_OUTPUT_INPUT,PROJECT_OUTPUT,['process.execute']),
    ('project_configure','project.configure','Select and save a project task profile. Cargo evaluates format-version 1 metadata; Maven effective POM and Gradle init-model export evaluate actual project rules and may resolve dependencies. Shader configuration validates typed options without invoking its compiler. These are trusted executable project inputs.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_build','project.build','Build Cargo, npm, standalone Java, Maven package (skip tests), Gradle build, or one selected HLSL/GLSL shader with installed DXC/glslang. Shader typed options select stage/entry/model/target/macros/includes, with bounded fresh DXIL/SPIR-V outputs and real diagnostics. Refuses unsaved live buffers; executes actual trusted tooling.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_test','project.test','Run Cargo tests, npm test, Maven test or Gradle test with real project framework configuration, bounded output/deadline and process-tree cancellation. Standalone Java has no framework runner.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_install','project.install','Explicitly install this npm project dependencies using npm ci when a package-lock exists, otherwise npm install. Runs trusted project lifecycle scripts; may access package registries and write lock/dependency files.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_script','project.script','Execute one existing npm script or one simple Maven goal/Gradle task with bounded output/deadline. Use project_launch for application execution; no shell command concatenation.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_launch','project.launch','Build/run a Cargo or Java main target, including Maven/Gradle resolved runtime classpaths, or start an npm script in a persistent PTY. Actual class-file main methods determine Java targets; Maven runtime metadata selects one module directory. No browser or Java debugger is launched.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_debug_target','project.debug','Build the selected Cargo executable or Java/Maven/Gradle main class. Cargo returns program; JVM targets return debugProfile selecting builtin.java with the actual main class, Java executable and resolved runtime classpaths. Pass it to debug_configure/debug_launch. Does not start a debugger itself.',schema(PROJECT_FIELDS),PROJECT_RESULT,['process.execute']),
    ('project_cancel','project.cancel','Cancel the active native project compilation/test/script process tree. Does not close a persistent run terminal; use terminal_close with its id.',schema(),schema({'cancelled':BOOL},('cancelled',)),['process.execute']),
    ('msbuild_inspect','msbuild.inspect','Inspect a .sln/.slnx or direct MSBuild project, solution-to-project configurations and saved artifacts/index. Never evaluates project imports or builds targets.',schema(MSBUILD_FIELDS),MSBUILD_MODEL,[]),
    ('msbuild_tools','msbuild.tools','Detect Visual Studio MSBuild and installed .NET SDKs without loading project rules.',schema({'refresh':BOOL}),schema({'tools':ARRAY},('tools',)),[]),
    ('msbuild_build','msbuild.build','Build the selected trusted .sln/.slnx/project with real MSBuild; solution project selection honors nesting/configuration mappings. Optional restore invokes package restore. Saves the successful local profile. .slnx requires a sufficiently recent build engine.',schema({**MSBUILD_FIELDS,'restore':BOOL}),MSBUILD_RESULT,['process.execute']),
    ('msbuild_rebuild','msbuild.rebuild','Rebuild the selected trusted solution/project, including its clean target. Runs project rules.',schema({**MSBUILD_FIELDS,'restore':BOOL}),MSBUILD_RESULT,['process.execute']),
    ('msbuild_clean','msbuild.clean','Run the Clean target of the selected trusted solution/project. Build rules determine which generated outputs are removed.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('msbuild_restore','msbuild.restore','Run package restore for the trusted solution/project through MSBuild and configured package sources.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('msbuild_evaluate','msbuild.evaluate','Explicitly evaluate one trusted workspace project with MSBuild 17.8+ and read real target properties/items. Loads imports/property functions and can execute project code; this is not a read-only XML parser.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('msbuild_prepare','msbuild.prepare','Explicitly evaluate up to eight enabled local VC++ projects and prepare a managed clangd compilation database from evaluated includes/defines/forced includes/language modes. Does not compile targets. AdditionalOptions are reported for review.',schema(MSBUILD_FIELDS),MSBUILD_RESULT,['process.execute']),
    ('vcs_inspect','vcs.inspect','Read branches, remotes, tracking divergence and stashes without changing the repository.',schema(),VCS_INSPECT,[]),
    ('vcs_init','vcs.init','Initialize a Git repository in the selected existing workspace directory with an initial main branch; refuses existing Git markers and bare repositories.',schema(),PROCESS,['vcs.write']),
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
    ('debug_adapters','debug.adapters','List native/plugin adapter identities without starting/probing them. builtin preserves native/.NET selection; builtin.java launches via a configured JDT LS with the Microsoft java-debug add-on and a host-owned loopback DAP connection. Installation/readiness is not implied.',schema(),schema({'revision':REVISION,'adapters':{'type':'array','maxItems':34,'items':schema({'id':STRING,'title':STRING,'type':STRING,'plugin':STRING,'requests':{'type':'array','items':{'type':'string','enum':['launch','attach']}}},('id','title','type','plugin','requests'))}},('revision','adapters')),[]),
    ('debug_profile','debug.profile','Read the persistent debug/run profile. adapterId selects builtin native/.NET, builtin.java or an active plugin ID. Java launch options use adapterOptions; the independent javaAttach object stores JDWP hostName, port and timeout milliseconds. The profile can contain environment variables. Legacy profiles retain default local endpoint values.',schema(),debug_schema(DEBUG_PROFILE_REQUIRED),['process.execute']),
    ('debug_configure','debug.configure','Update and atomically save supported debug profile fields in .cycode/launch.json, including the typed javaAttach endpoint. Optional expectedProfile must match the complete observed native profile; mismatch rejects the update without writing. Optional unrealLaunch persists the bound build/launch context; set it null to detach. Bound program/cwd/args/runtime are checked by the common Unreal owner before saving or launch. Existing disk baseline guards still apply. Arguments are literal argv. No compiler, DNS lookup, language server, network connection or debug session starts during configuration.',DEBUG_CONFIGURE_INPUT,debug_schema(DEBUG_PROFILE_REQUIRED),['process.execute','workspace.write']),
    ('debug_status','debug.status','Read actual DAP state, verified breakpoints, threads, stack, scopes, variables, watches and bounded adapter output. Source lines/columns are one-based. Runtime identifiers expire on resume/relaunch.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_breakpoints','debug.breakpoints','Replace source breakpoints for one workspace file and persist them. Supports enabled flags, conditions, hit counts and logpoints according to adapter capabilities. expectedBreakpoints rejects concurrent changes.',
     schema({'path':PATH,'breakpoints':BREAKPOINTS,'expectedBreakpoints':BREAKPOINTS},('path','breakpoints')),DEBUG_STATE,['process.execute','workspace.write']),
    ('debug_launch','debug.launch','Launch through the shared DAP owner. builtin.java requires enabled JDT LS with the Microsoft java-debug bundle and explicit mainClass/classPaths; it obtains the adapter port from the server and connects only to IPv4 loopback. Reconfiguration invalidates the session. Native/.NET and contributed stdio adapters retain their owners. Supplied fields override the saved profile; this operation does not build or install tools.',debug_schema(),DEBUG_STATE,['process.execute']),
    ('debug_attach','debug.attach','Attach through the shared DAP owner. Set adapterId=builtin.java for JDWP: javaAttach supplies hostName, port and timeout (100-10000 ms), or uses the saved/default endpoint. Java rejects pid and needs enabled JDT LS plus java-debug, without build/main class/classpath requirements. Local JDT LS connects to the explicit local/remote JVM; the DAP socket stays loopback. Other adapters require a positive pid. Stop detaches and leaves existing targets running; no target terminal is started for Java attach.',DEBUG_ATTACH_INPUT,DEBUG_STATE,['process.execute']),
    ('debug_run','debug.run','Run a trusted native executable or runnable .NET assembly in a native terminal without debugging. Java uses project_launch instead. Uses the saved profile and optional overrides; terminal ownership lasts until explicitly closed or workspace exit.',debug_schema(),schema({'terminalId':STRING,'pid':{'type':'integer'}},('terminalId','pid')),['process.execute']),
    ('debug_continue','debug.continue','Continue the paused debuggee in the selected thread. Optional observed sessionGeneration/stopGeneration/threadId guards reject changed contexts; omissions capture the current context when queued. singleThread requires supportsSingleThreadExecutionRequests. Granularity is not a Continue argument. Returns actual optional allThreadsContinued in lastExecution; acceptance is distinct from a later stop. Not cancellable after submission.',schema({'singleThread':BOOL}),DEBUG_STATE,['process.execute']),
    ('debug_pause','debug.pause','Pause the running debuggee and asynchronously inspect its actual threads/stack.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_hot_reload','debug.hotReload','Manually compile the imported JDT workspace incrementally (java/buildWorkspace=false), then send java-debug redefineClasses on the same Java DAP session. Requires sessionGeneration from debug_status and saved/conflict-free open documents unchanged during compilation. Build errors/cancellation prevent application. Returns javaReload with actual class names or unchanged; DAP success with errorMessage is failure. JDT source/output mapping and JVM redefine support are prerequisites. Does not enable imports/autobuild or restart a JVM. Applying changes is not undoable; a failed/expired request may have partially applied changes.',schema({'sessionGeneration':{'type':'integer','minimum':1},'operationId':{'type':'string','minLength':1,'maxLength':128,'pattern':'^[A-Za-z0-9_.-]+$','not':{'pattern':'[^A-Za-z0-9_.-]'}}},('sessionGeneration',)),DEBUG_STATE,['process.execute']),
    ('debug_cancel_reload','debug.cancelReload','Signal cancellation before Java redefineClasses is sent. Optional operationId matches a pending reload (including a queued one); omission targets the reload in debug_status. Cancels the owned JDT build request without restarting/stopping the Java server or JVM. Does not undo changes or cancel an already submitted VM redefine request. Refresh debug_status for the final outcome.',schema({'operationId':{'type':'string','minLength':1,'maxLength':128,'pattern':'^[A-Za-z0-9_.-]+$','not':{'pattern':'[^A-Za-z0-9_.-]'}}}),DEBUG_STATE,['process.execute']),
    ('debug_next','debug.next','Step over in the observed selected thread. Optional granularity (statement/line/instruction) requires supportsSteppingGranularity; omission uses the adapter default without sending that field. singleThread requires supportsSingleThreadExecutionRequests. Optional observed context rejects stale calls; omitted context is captured when queued. Actual stopped events supply the resulting stack. Not cancellable after submission.',schema(DEBUG_STEP_OPTIONS),DEBUG_STATE,['process.execute']),
    ('debug_step_in','debug.stepIn','Step into through the common DAP owner, with optional capability-gated granularity/singleThread and queued or explicit session/stop/thread context. Target selection within a source line remains separate; no caller targetId. lastExecution distinguishes applying/accepted/unconfirmed; this action has no automatic Undo or retry.',schema(DEBUG_STEP_OPTIONS),DEBUG_STATE,['process.execute']),
    ('debug_step_out','debug.stepOut','Step out through the common DAP owner, with optional capability-gated granularity/singleThread and queued or explicit session/stop/thread context. Returns acknowledgement state; waits for actual adapter events to refresh frames. No automatic Undo, cancellation after submission or retry.',schema(DEBUG_STEP_OPTIONS),DEBUG_STATE,['process.execute']),
    ('debug_step_back','debug.stepBack','Execute one backward step through a debugger advertising supportsStepBack. Requires explicit observed sessionGeneration/stopGeneration/threadId and a paused selected thread. Optional granularity/singleThread require their own capabilities. The adapter must provide reverse/recorded execution; CyCode does not synthesize target history. Acknowledgement does not prove the next stop completed. No automatic retry or cancellation after submission.',
     schema({**DEBUG_EXECUTION_CONTEXT,**DEBUG_STEP_OPTIONS},tuple(DEBUG_EXECUTION_CONTEXT)),DEBUG_STATE,['process.execute']),
    ('debug_reverse_continue','debug.reverseContinue','Continue backward through supportsStepBack on the observed paused selected thread. Requires explicit sessionGeneration/stopGeneration/threadId. Optional singleThread requires supportsSingleThreadExecutionRequests; granularity is not accepted. Uses actual backend reverse execution and reports optional actual allThreadsContinued. Failed/expired requests may have effects; inspect debug_status.lastExecution before another action. Not cancellable after submission.',
     schema({**DEBUG_EXECUTION_CONTEXT,'singleThread':BOOL},tuple(DEBUG_EXECUTION_CONTEXT)),DEBUG_STATE,['process.execute']),
    ('debug_restart_frame','debug.restartFrame','Restart the observed selected frame through supportsRestartFrame. Requires explicit sessionGeneration/stopGeneration/frameId; rejects a changed/missing frame or canRestart=false. Sends one restartFrame and awaits the actual stopped event; no automatic stepIn or VM restart. Re-execution can repeat program side effects and does not undo I/O. Not source Undo, cancellable after submission or automatically retried.',
     schema({'sessionGeneration':DEBUG_EXECUTION_CONTEXT['sessionGeneration'],'stopGeneration':NONNEGATIVE,
             'frameId':{'type':'integer','minimum':0,'maximum':2147483647}},('sessionGeneration','stopGeneration','frameId')),DEBUG_STATE,['process.execute']),
    ('debug_step_in_targets','debug.stepInTargets','Read actual step-in targets for the observed selected paused frame through supportsStepInTargetsRequest. Requires explicit sessionGeneration/stopGeneration/threadId/frameId. Returns at most 128 host-issued handles and actual labels/ranges (one-based UTF-16 coordinates; zero means unknown). Raw signed adapter IDs stay private. Refresh replaces the sole catalogue; handles expire on resume, a new stop, invalidation, capability/frame/thread/session changes or execution submission. Empty targets is an actual adapter result. Bounds: 4 KiB request, 512 KiB reply, 4096-byte labels; no arbitrary DAP request.',
     schema(DEBUG_STEP_TARGET_CONTEXT,tuple(DEBUG_STEP_TARGET_CONTEXT)),DEBUG_STEP_TARGET_CATALOGUE,['process.execute']),
    ('debug_step_into_target','debug.stepIntoTarget','Enter an explicitly chosen host-issued target from debug_step_in_targets through the shared DAP stepIn owner. Requires the same observed sessionGeneration/stopGeneration/threadId/frameId and target handle. Optional granularity/singleThread need advertised capabilities. Consumes the complete catalogue before submission; no raw targetId, fabricated target, automatic retry, rollback, source Undo or cancellation after submission. Inspect debug_status.lastExecution for accepted/unconfirmed acknowledgement and wait for actual stopped events. Public execution arguments retain the issued handle, never the adapter ID.',
     schema({**DEBUG_STEP_TARGET_CONTEXT,'target':DEBUG_STEP_TARGET_HANDLE,**DEBUG_STEP_OPTIONS},
            (*DEBUG_STEP_TARGET_CONTEXT,'target')),DEBUG_STATE,['process.execute']),
    ('debug_stop','debug.stop','Stop the debug session. Terminates launched debuggees and their owned terminals; detaches from an explicitly attached process, leaving that existing process running.',schema(),DEBUG_STATE,['process.execute']),
    ('debug_run_to_cursor','debug.runToCursor','Continue normally to a verified destination line using a session-only source breakpoint lease. Requires explicit observed sessionGeneration/stopGeneration/threadId, workspace path/one-based line, expectedDocumentId/expectedRevision from documents_read and expectedBreakpoints from debug_status.breakpoints[path] (empty if absent). Native source identity/revision/clean disk bytes are checked before arming and again before continuing; this does not prove source matches the compiled binary. Preserves all configured source breakpoints, reuses an unconditional destination breakpoint, rejects conditional/logpoint collisions and relocated/unverified/ambiguous verification. Optional singleThread needs its adapter capability; no granularity or arbitrary DAP fields. One continue acknowledgement appears in lastExecution; runToCursor reports lease/restoration and actual/unknown target hit. Restores the exact original list at the next actual stop, including earlier breakpoints/exceptions/pause. Late requests must settle before restoration; failed/uncertain cases stay leased until explicit repair or session stop. No saved breakpoint edits, cancellation after submission, automatic resume/retry or source Undo. Bounds: 1 MiB public request, 1000 total breakpoints including a new temporary point, 512 KiB reply, 256 deferred events/1 MiB/64 KiB each.',
     schema({**DEBUG_EXECUTION_CONTEXT,'path':{'type':'string','minLength':1,'maxLength':8192,'not':{'pattern':'\u0000'}},
        'line':{'type':'integer','minimum':1,'maximum':10000000},'expectedDocumentId':{'type':'integer','minimum':1},
        'expectedRevision':{'type':'integer','minimum':1},'expectedBreakpoints':BREAKPOINTS,'singleThread':BOOL},
        (*DEBUG_EXECUTION_CONTEXT,'path','line','expectedDocumentId','expectedRevision','expectedBreakpoints')),DEBUG_STATE,['process.execute']),
    ('debug_restore_run_to_cursor','debug.restoreRunToCursor','Explicitly restore the run-to-cursor lease original source breakpoint list after an uncertain failure. Requires observed sessionGeneration/stopGeneration and expectedRevision from debug_status.runToCursor.revision, an actual confirmed stop and no outstanding arm/continue/restore replies. Sends one full original-list replacement and validates its response. Does not resume, edit saved breakpoints, automatically retry or cancel an in-flight request. Pause first when the stopped state is uncertain; stopping the debug session also ends the lease. Noncancellable after submission.',
     schema({'sessionGeneration':DEBUG_EXECUTION_CONTEXT['sessionGeneration'],'stopGeneration':NONNEGATIVE,'expectedRevision':NONNEGATIVE},
        ('sessionGeneration','stopGeneration','expectedRevision')),DEBUG_STATE,['process.execute']),
    ('debug_goto_targets','debug.gotoTargets','Read actual valid next-statement destinations in the selected frame source through supportsGotoTargetsRequest. Requires explicit sessionGeneration/stopGeneration/threadId/frameId, one-based line and optional UTF-16 column (both at most 10000000). Source path/reference and opaque adapterData come only from the actual selected frame; no caller path/source/targetId. Returns one replaceable catalogue with up to 128 issued handles, actual labels/required lines/optional ranges and instruction references; raw signed adapter IDs and adapterData stay private. Rechecks the complete source request after the reply, including adapterData. Bounds: public request 4 KiB, frame source 64 KiB, reply 512 KiB, labels 4096 UTF-8 bytes. Empty targets is an actual result. This only queries; it does not move or continue the program.',
     schema({**DEBUG_STEP_TARGET_CONTEXT,'line':DEBUG_GOTO_COORDINATE,'column':DEBUG_GOTO_COORDINATE},(*DEBUG_STEP_TARGET_CONTEXT,'line')),DEBUG_GOTO_CATALOGUE,['process.execute']),
    ('debug_goto_target','debug.gotoTarget','Set the next statement through one DAP goto, using an issued debug_goto_targets handle and the same explicit observed sessionGeneration/stopGeneration/threadId/frameId. This skips intervening code or permits its repetition; it does NOT run to the cursor, continue the program or undo program side effects. No granularity/singleThread, raw targetId, arbitrary source/command, automatic retry or source Undo. Consumes both target catalogues before submission. Acknowledgement is recorded as accepted/unconfirmed in lastExecution using only public handles. pendingStop blocks inspection/execution until the actual relocated stopped event; a missing event after 15 seconds reports unconfirmed without automatic continuation. Not cancellable after submission.',
     schema({**DEBUG_STEP_TARGET_CONTEXT,'target':DEBUG_STEP_TARGET_HANDLE},(*DEBUG_STEP_TARGET_CONTEXT,'target')),DEBUG_STATE,['process.execute']),
    ('debug_evaluate','debug.evaluate','Evaluate an expression or debugger console command in the selected stopped frame. Expressions/console commands can have side effects in the target process.',schema({'expression':{'type':'string','maxLength':8192},'context':{'type':'string','enum':['watch','hover','repl']}},('expression',)),{'type':'object'},['process.execute']),
    ('debug_variables','debug.variables','Read a bounded page of children or register-scope entries. References become stale on resume; stopGeneration pins the observed stop.',schema({'variablesReference':{'type':'integer','minimum':1,'maximum':2147483647},'start':{'type':'integer','minimum':0,'maximum':10000000},'count':{'type':'integer','minimum':1,'maximum':200},'filter':{'type':'string','enum':['named','indexed']}},('variablesReference',)),schema({'variables':ARRAY,'stopGeneration':NONNEGATIVE,'start':NONNEGATIVE,'count':NONNEGATIVE,'hasMore':BOOL},('variables','stopGeneration','start','count','hasMore')),['process.execute']),
    ('debug_memory','debug.memory','Read up to 4096 bytes through the selected DAP adapter when it advertises memory reads. Returns validated base64, decoded bytes and unreadable count inside body. Requires a stopped live target; no memory write.',schema({'memoryReference':{'type':'string','minLength':1,'maxLength':8192},'offset':{'type':'integer','minimum':-1048576,'maximum':1048576},'count':{'type':'integer','minimum':1,'maximum':4096}},('memoryReference',)),DEBUG_INSPECTION,['process.execute']),
    ('debug_set_variable','debug.setVariable','Assign a named child of a variablesReference through supportsSetVariable. Requires explicit observed sessionGeneration/stopGeneration and a paused target. Prefer debug_set_expression when the variable has evaluateName and supportsSetExpression. value is an adapter/language expression, including an empty string. Returns actual adapter value/type in lastMutation; old inspection references are invalidated after every attempted change. Failures may have side effects: refresh debug_status for rejected/unconfirmed state. Changes affect the debuggee, are not saved to source, undoable or cancellable once submitted.',
     schema({**DEBUG_MUTATION_GUARD,'variablesReference':{'type':'integer','minimum':1,'maximum':2147483647},'name':DEBUG_MUTATION_TARGET,'value':DEBUG_MUTATION_TEXT},('sessionGeneration','stopGeneration','variablesReference','name','value')),DEBUG_STATE,['process.execute']),
    ('debug_set_expression','debug.setExpression','Assign an expression through supportsSetExpression in the observed selected frameId. Requires explicit sessionGeneration/stopGeneration and a paused target; rejects a changed frame instead of assigning in the new context. Returns adapter value/type in lastMutation and invalidates inspection after an attempt. Expressions may execute target code; there is no automatic Undo, retry, source edit or cancellation after submission. On transport/adapter failure inspect debug_status.lastMutation before any new attempt.',
     schema({**DEBUG_MUTATION_GUARD,'frameId':{'type':'integer','minimum':0,'maximum':2147483647},'expression':DEBUG_MUTATION_TARGET,'value':DEBUG_MUTATION_TEXT},('sessionGeneration','stopGeneration','frameId','expression','value')),DEBUG_STATE,['process.execute']),
    ('debug_write_memory','debug.writeMemory','Write 1 to 4096 explicit bytes at an opaque memoryReference and optional offset. Requires observed sessionGeneration/stopGeneration, paused target, and supportsReadMemoryRequest plus supportsWriteMemoryRequest. expectedBytes must have the same length as bytes and match a fresh native read before submitting writeMemory with allowPartial=false. This comparison is not atomic. Re-reads afterward; lastMutation distinguishes applied/readBackVerified, partial, rejected and unconfirmed. bytesWritten is exposed only when actually reported by the adapter. Inspection expires after each attempt. Target changes have no automatic Undo, rollback, retry or cancellation once submitted.',
     schema({**DEBUG_MUTATION_GUARD,'memoryReference':{**DEBUG_MUTATION_TEXT,'minLength':1},'offset':DEBUG_MEMORY_OFFSET,'bytes':DEBUG_MEMORY_BYTES,'expectedBytes':DEBUG_MEMORY_BYTES},('sessionGeneration','stopGeneration','memoryReference','bytes','expectedBytes')),DEBUG_STATE,['process.execute']),
    ('debug_disassemble','debug.disassemble','Read up to 200 actual instructions through a capable stopped DAP adapter. Byte/instruction offsets are bounded; source locations remain subject to normal navigation permissions.',schema({'memoryReference':{'type':'string','minLength':1,'maxLength':8192},'offset':{'type':'integer','minimum':-1048576,'maximum':1048576},'instructionOffset':{'type':'integer','minimum':-4096,'maximum':4096},'instructionCount':{'type':'integer','minimum':1,'maximum':200}},('memoryReference',)),DEBUG_INSPECTION,['process.execute']),
    ('debug_exception_info','debug.exceptionInfo','Inspect the actual exception reported for the selected stopped thread, if supported by this adapter.',schema({'threadId':{'type':'integer','minimum':1,'maximum':2147483647}}),DEBUG_INSPECTION,['process.execute']),
    ('debug_data_breakpoint_info','debug.dataBreakpointInfo','Inspect a potential data breakpoint in the selected paused stack frame, or a named child of variablesReference. Requires advertised supportsDataBreakpoints plus observed sessionGeneration/stopGeneration. Returns a host-issued stop-bound handle and actual accessTypes/canPersist metadata; null adapter dataId becomes available:false. Raw dataId never leaves the native owner. Handles expire on resume, another stop, invalidation or disconnect; installed breakpoints can survive resume. Bounds: 128 handles / 1 MiB per pause.',
     schema({'name':{**DATA_TEXT,'minLength':1},'variablesReference':{'type':'integer','minimum':1,'maximum':2147483647},'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE},('name','sessionGeneration','stopGeneration')),DATA_INFO,['process.execute']),
    ('debug_data_breakpoints','debug.dataBreakpoints','Replace the entire session-owned data breakpoint list (empty clears). Requires the current paused sessionGeneration/stopGeneration and expectedRevision from debug_status.dataBreakpointRevision. Every target needs a freshly issued handle at this pause; no raw dataId, stale handle reuse, implicit old-list merge or cross-session persistence. Uses only advertised access/condition/hit capabilities. Up to 32 entries; actual verified flags/messages remain visible. A rejected/timed-out/interrupted mutation may have side effects: dataBreakpointsSynchronized=false retains only the last known list; re-inspect and replace/clear while paused. Not undoable or cancellable once submitted to the adapter.',
     schema({'breakpoints':DATA_POINTS,'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE,'expectedRevision':NONNEGATIVE},('breakpoints','sessionGeneration','stopGeneration','expectedRevision')),DEBUG_STATE,['process.execute']),
    ('debug_instruction_breakpoints','debug.instructionBreakpoints','Replace the complete session-only instruction breakpoint list (empty clears). Requires supportsInstructionBreakpoints and explicit paused sessionGeneration/stopGeneration plus expectedRevision from debug_status.instructionBreakpointRevision. Entries use adapter instruction/memory references or actual disassembly addresses, optional byte offset within 1 MiB, enabled and capability-gated condition/hitCondition. Disabled entries are retained locally with verification:null and omitted from DAP; active entries retain actual verification, optional relocated address/offset and pending/failed reason. Bounds: 128 entries / 1 MiB request, 512 KiB reply; exact duplicate reference/offset pairs are rejected natively. Events received before acknowledgement are bounded and reconciled by actual IDs. Failures/late effects leave instructionBreakpointsSynchronized=false with the last known list; inspect status then explicitly replace/clear. References are caller-reviewed for this pause, never persisted/rebased across launches. No automatic retry, Undo or cancellation after submission.',
     schema({'breakpoints':INSTRUCTION_POINTS,'sessionGeneration':{'type':'integer','minimum':1},'stopGeneration':NONNEGATIVE,'expectedRevision':NONNEGATIVE},('breakpoints','sessionGeneration','stopGeneration','expectedRevision')),DEBUG_STATE,['process.execute']),
    ('debug_function_breakpoints','debug.functionBreakpoints','Replace and persist up to 128 function breakpoints. The live adapter must advertise function/condition/hit-count support. expected rejects conflicting edits; disabled entries do not bind.',schema({'breakpoints':FUNCTION_BREAKPOINTS,'expected':FUNCTION_BREAKPOINTS},('breakpoints',)),DEBUG_STATE,['process.execute','workspace.write']),
    ('debug_exception_breakpoints','debug.exceptionBreakpoints','Persist exception filter IDs advertised by the actual adapter. filters=null restores adapter defaults, [] disables all filters. expected rejects conflicting edits.',schema({'filters':EXCEPTION_FILTERS,'expected':EXCEPTION_FILTERS},('filters',)),DEBUG_STATE,['process.execute','workspace.write']),
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
    ('language_symbols', 'language.symbols', 'Search actual configured project symbols, optionally filtered to types. language defaults to cpp; other backends require language_configure. Unsupported server methods are reported explicitly. Index completeness depends on background analysis. Returns bounded normalized symbols with snapshot-local node/parent/depth, fullRange, deprecation, backend/epoch and invalid-row count. Server configurations changing during the request are rejected; expectedEpoch optionally guards the caller snapshot. Workspace positions remain index observations, not current document ranges. Up to 1000 rows, 8192 visited records, nesting 64 and 4 MiB normalized symbol-array payload; malformed rows counted separately from unavailable/outside sources. URI-only workspace symbols retain their scoped path and issued session/resolveIndex without invented ranges. Resolve on selection with language_symbol_resolve; raw server data stays native. Four sessions, two-minute expiry, 1000 items, 64 KiB per raw item and 1 MiB raw/resolved cache per session; oversized raw rows count invalid. The normalized list retains its 4 MiB bound.',
     schema({'query': {'type': 'string', 'maxLength': 256}, 'expectedEpoch':NONNEGATIVE, 'language':{'type':'string','minLength':1,'maxLength':160}, 'typesOnly': BOOL, 'includeExternal': BOOL, 'limit': {'type': 'integer', 'minimum': 1, 'maximum': 1000}}), SYMBOLS, []),
    ('language_symbol_resolve', 'language.symbolResolve', 'Resolve the location.range of one issued URI-only workspace symbol through its original backend. Accepts only a native-issued session and resolveIndex, never caller-supplied server data. Same workspace edit generation/configuration epoch required before and after the reply; expired/evicted sessions require a new search. Original name/kind/URI are verified; only the resolved range is accepted. Workspace/registered SDK source guards remain. Positions remain index observations. Successful bounded replies are cached within the original session budget/TTL; no file edits, server activation, commands or arbitrary external reads.',
     schema({'session':SYMBOL_SESSION,'index':{'type':'integer','minimum':0,'maximum':999},'expectedEpoch':NONNEGATIVE},('session','index')),
     schema({'symbol':SYMBOL,'backend':STRING,'epoch':NONNEGATIVE,'positionEncoding':{'const':'utf-16'}},('symbol','backend','epoch','positionEncoding')), []),
    ('language_document_symbols', 'language.documentSymbols', 'Read document structure from the live buffer, preserving hierarchy as snapshot-local node/parent/depth and fullRange alongside the legacy navigation range. Includes documentId/revision, backend/epoch, deprecation and invalid-row count. Accepts a workspace path or an opened SDK reference. Optional expectedDocumentId/expectedEpoch guard the caller snapshot. Replies require the same open document identity/revision and language epoch; unrelated document edits no longer invalidate this query. Selection/body coordinates must belong to the live UTF-16 buffer without splitting surrogate pairs. Same 1000-row/8192-record/depth-64/4-MiB symbol-array bounds as language_symbols; filtered or malformed parents can leave represented descendants as roots. No independent parser or symbol inference.',
     schema({'path': PATH, 'expectedRevision': REVISION, 'expectedDocumentId':NONNEGATIVE, 'expectedEpoch':NONNEGATIVE, 'includeExternal': BOOL, 'limit': {'type': 'integer', 'minimum': 1, 'maximum': 1000}}, ('path', 'expectedRevision')), DOCUMENT_SYMBOLS, []),
    ('sources_read', 'sources.read', 'Read an immutable SDK source view. Requires a sdk:// reference issued by symbol navigation. Arbitrary external paths and files outside registered Unreal/compiler SDK source roots are rejected.',
     schema({'source': {'type': 'string', 'minLength': 1, 'maxLength': 4096, 'pattern': '^sdk://[0-9]+/.+$'}}, ('source',)), SOURCE_DOCUMENT, []),
    ('language_rename', 'language.rename', 'Prepare compiler-issued semantic rename edits. C++ first analyses all workspace source-spelled candidates, including declarations, callers and generated C++ files; compiler-confirmed local parameters avoid a whole-tree scan. Live revisions and the discovery scope are guarded through apply. More than 128 preparation candidates refuses without issuing a partial preview. Inspect coverage.strategy and limitations; this does not claim exhaustive macro/token-paste, serialized Unreal asset or external-project refactoring. Does not edit or save source text. Slow preparation exposes SDK progress and supports owner cancellation.',
     schema({'path': PATH, 'expectedRevision': REVISION, 'position': POSITION, 'newName': {'type': 'string', 'minLength': 1, 'maxLength': 256}}, ('path', 'expectedRevision', 'position', 'newName')), CHANGE, []),
    ('language_format', 'language.format', 'Prepare formatting edits from the selected language server. Does not apply changes or save files. A range limits formatting to the selection.',
     schema({'path': PATH, 'expectedRevision': REVISION, 'range': RANGE, 'tabSize': {'type': 'integer', 'minimum': 1, 'maximum': 16}, 'insertSpaces': BOOL}, ('path', 'expectedRevision')), CHANGE, []),
    ('changes_preview', 'changes.preview', 'Read before/after text and concrete UTF-16 edits for one file in a prepared change. Preview IDs belong to the running IDE and expire; they are not filesystem paths.',
       schema({'id': STRING, 'path': PATH}, ('id', 'path')), schema({'id': STRING, 'path': PATH, 'before': STRING, 'after': STRING, 'edits': ARRAY,'annotations':CHANGE_ANNOTATIONS}, ('id', 'path', 'before', 'after', 'edits')), []),
    ('changes_apply', 'changes.apply', 'Apply a prepared change to live workspace buffers only. Inspect annotations in the plan/preview; explicitly pass every needsConfirmation ID in confirmedAnnotations. IDs belong to that issued plan and cannot be reused from another. Unknown/duplicate/missing confirmations fail before any edit. Every affected revision and disk baseline is validated before any buffer changes. Does not save to disk.',
       schema({'id': STRING,'confirmedAnnotations':{'type':'array','maxItems':128,'uniqueItems':True,'items':ANNOTATION_ID}}, ('id',)), CHANGE, ['workspace.write']),
    ('changes_undo', 'changes.undo', 'Undo a previously applied refactoring across its live buffers. Refuses changed/closed buffers and external disk conflicts. Does not save to disk.',
     schema({'id': STRING}, ('id',)), CHANGE, ['workspace.write']),
    ('unreal_prepare', 'unreal.prepare', 'Prepare and cache Unreal code analysis, including UHT-generated headers; reuses a matching existing cache. Executes trusted project rules.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('unreal_inspect', 'unreal.inspect', 'Inspect an Unreal .uproject, engine, targets, modules and plugins without executing project rules.', schema(UNREAL_FIELDS), UNREAL_MODEL, []),
    ('unreal_artifacts','unreal.artifacts','Inspect ordinary UE4/UE5 desktop Editor outputs in Development/DebugGame without executing tools or loading binaries. Reads the selected engine shared .modules BuildId, project Editor .target context/type/project/optional launch/BuildId, project-root and receipt-listed project-owned module manifests, and receipt/manifest-listed project binaries. Requires a project receipt for declared or Source-owned C++ modules; content-only projects do not require one. Ignores engine build-product/dependency inventories, symbols/resources, plugin activation/module filters, unlisted plugins and ABI/architecture loading; ready means only that the scoped metadata/file-presence checks found no issue. Bounds: per-file 4 MiB, metadata aggregate 16 MiB/256 files, 8192 receipt products, 4096 recorded entries, 128 issues, cooperative five-second budget. Unknown variables, escaping paths, malformed metadata, races or exhausted bounds remain unverified. No repair/rebuild/delete/installation or content snapshot. Bound actual run/launch and desktop prebuild continuation use fresh native assessment; configuration remains possible before compilation.',schema(UNREAL_FIELDS),UNREAL_ARTIFACTS,[]),
    ('unreal_launch_profile','unreal.launchProfile','Prepare a shared debug profile patch for an installed UE4/UE5 editor, standalone game (-game) or uncooked dedicated server (-server). Uses the current host platform and Development or DebugGame (requires the installed configuration-specific editor executable), an optional existing /Game map, typed listen/port/window/resolution/log flags and build/entry options. Listen requires an explicit map. Build-before-debug requires a discovered Editor target; disable for content-only projects. Reads files only and starts/saves nothing. Requires process.execute like debug_profile because expectedProfile includes the saved environment. Review settings and send them with returned expectedProfile to debug_configure, then use debug_run or debug_launch; those SDK calls do not build automatically. Persists unrealLaunch with the exact Unreal build profile and normalized launch options, and preserves other saved fields such as watches/environment when configuring the patch. Desktop build-before-debug uses the bound target/configuration, independent of toolbar changes; SDK calls do not compile. The preview is not an execution permit or a frozen engine/content snapshot; actual launch validates executable availability through the shared owner. No cooked-server/game binary, client connection URL, PIE, Live Coding or Blueprint debugging.',UNREAL_LAUNCH_INPUT,UNREAL_LAUNCH_RESULT,['process.execute']),
    ('unreal_generate', 'unreal.generate', 'Generate Unreal solution/project files using the selected engine UnrealBuildTool. Executes trusted project rules.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('unreal_database', 'unreal.database', 'Generate compile_commands.json for clangd via UnrealBuildTool. Executes trusted project rules.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('unreal_build', 'unreal.build', 'Compile a discovered Unreal target with UnrealBuildTool after saving or reconciling workspace documents; does not cook or package assets. Uses the explicit project/engine/target/platform/configuration parameters. SDK launch operations do not perform this build automatically.', schema(UNREAL_FIELDS), UNREAL_RESULT, ['process.execute']),
    ('workspace_describe', 'workspace.describe', 'Describe the live workspace.', schema(),
     schema({'root': STRING, 'name': STRING, 'projectType': STRING, 'version': STRING, 'solution': {'type': ['object', 'null']}}, ('root', 'name', 'projectType', 'version', 'solution')), []),
    ('workspace_counterparts','workspace.counterparts','Find C/C++ header/source companion files for an already-open saved workspace document. Requires current expectedRevision and expectedDocumentId; rechecks source lifetime/revision after metadata probes. No LSP, compiler, ripgrep, project-rule execution, recursive scan, source parsing, file edit/save or implicit document opening. Supports h/hh/hpp/hxx/inl versus c/cc/cpp/cxx/m/mm, and excludes .generated/.gen stems. Checks siblings, then the nearest Public/Private/Classes or include/includes versus src/source layout, retaining nested paths and paired-root fallbacks. Local probes try lower/upper extension variants. Also consults the already-current workspace file cache through a per-stem header/source offset index built during ordinary workspace_files refresh, without scanning or starting that refresh. Cached paths support other layouts and mixed-case extensions. Cached paths respect the file index ignore rules; explicit local probes do not. Missing, busy or stale caches are not used; current partial caches may contribute candidates. Rechecks watcher revision and cache generation after probing; changes mark the report partial. index reports missing/busy/stale/ready/partial/changed, revision and generation; zero means unavailable. ready covers the existing cached scope, not ignored files or semantic completeness. Same-stem names across modules are ambiguous; no semantic pairing. At most 16 ancestors, 128 copied cache matches and 128 unique total metadata probes; truncated/unreadable observations are explicit. Existing regular canonical targets remain inside the workspace; symlink files and outside targets are excluded. Duplicates use case-insensitive Windows path comparison or case-sensitive POSIX comparison. The GUI auto-opens only one unambiguous result with a complete unchanged cache and no limits/unreadable paths; otherwise shows the common palette with index status. No cache activation, edits or opening by SDK/ABI2 callers. Results are momentary and ordinary guarded opening may still fail.',
     schema({'path':PATH,'expectedRevision':REVISION,'expectedDocumentId':NONNEGATIVE},('path','expectedRevision','expectedDocumentId')),
     schema({'path':PATH,'revision':REVISION,'documentId':NONNEGATIVE,
             'candidates':{'type':'array','maxItems':128,'items':schema({'path':PATH,'relation':{'type':'string','enum':['sibling','paired-directory','paired-root','workspace-index']}},('path','relation'))},
             'index':RELATED_INDEX,
             'checked':{'type':'integer','minimum':0,'maximum':128},'unreadable':{'type':'integer','minimum':0,'maximum':128},'truncated':BOOL},
            ('path','revision','documentId','candidates','index','checked','unreadable','truncated')),[]),
    ('workspace_include','workspace.include','Find workspace files matching the literal C/C++ #include on the caret logical directive of an already-open saved workspace document, including unsaved buffer changes. Requires expectedRevision, expectedDocumentId and a valid UTF-16 position; rechecks source lifetime/revision. Returns the literal target, quote/angle kind, exact inner-header UTF-16 range on the actual physical header line (which may differ from the caret line) and the requested position. A bounded native lexical context scan excludes comment/string/raw-string bodies and continued non-directive lines; it does not evaluate conditions or expand macros. Ordinary literal #include directives support inline block comments between tokens and immediate backslash-LF/CRLF continuations, including a spliced include keyword. The caret may lie on any continued component; the header-name itself must remain on one physical line with forward-slash relative paths. Nonspliced multiline comments between tokens and continued header-name literals are refused. Prefix limit 1 MiB, total logical directive span 8 KiB, target 1024 UTF-8 bytes; malformed/unsupported contexts fail explicitly. Supports preceding ordinary comments/strings, common raw delimiters and escaped-newline context; unrecognized raw delimiters are refused. For quoted headers, probes the source-relative path first. Both forms also use suffix-component matches from the already-current workspace file cache, with Windows case-insensitive or POSIX case-sensitive comparison. Parent segments are followed only by the quoted source-relative probe; no cache suffix guessing for such paths. The shared per-stem index stores bounded header/source/other offset buckets, supporting extensionless and .inc headers without another scan. At most 128 cached bucket entries and 128 total unique metadata probes. Missing/busy/stale caches contribute no paths; partial current caches can contribute retained paths. Rechecks watcher revision/cache generation after probing and document-lock reacquisition. index/truncated/unreadable explicitly describe incomplete or changed coverage. Existing canonical regular targets remain inside the workspace; symlink files and external targets are excluded. Duplicate local/index paths retain source-relative provenance. ready covers the cached ignore scope, not compiler include-search order or semantic completeness. Multiple candidates remain ambiguous; the GUI auto-opens only one complete unchanged result and otherwise uses the shared palette with caret/tab/source guards. No compiler include-directory interpretation, external SDK lookup, include_next/import, project rules, LSP, process, recursive enumeration, cache refresh, edit/save or implicit opening by SDK/ABI2 callers. Results are momentary observations.',
     schema({'path':PATH,'expectedRevision':REVISION,'expectedDocumentId':NONNEGATIVE,'position':POSITION},('path','expectedRevision','expectedDocumentId','position')),
     schema({'path':PATH,'revision':REVISION,'documentId':NONNEGATIVE,'position':POSITION,
             'include':schema({'target':{'type':'string','minLength':1,'maxLength':1024},'angled':BOOL,'range':RANGE},('target','angled','range')),
             'candidates':{'type':'array','maxItems':128,'items':schema({'path':PATH,'relation':{'type':'string','enum':['source-relative','workspace-index']}},('path','relation'))},
             'index':RELATED_INDEX,'checked':{'type':'integer','minimum':0,'maximum':128},'unreadable':{'type':'integer','minimum':0,'maximum':128},'truncated':BOOL},
            ('path','revision','documentId','position','include','candidates','index','checked','unreadable','truncated')),[]),
    ('workspace_files', 'workspace.files', 'Read a cached workspace path-index page, respecting ignore rules. Use nextOffset until null; refresh rebuilds the index. Each page reports both the watcher indexRevision and the actual cache rebuild generation. Validate both across pages: a manual rebuild may keep the same watcher revision while changing generation. A mismatch requires discarding or marking the assembly partial and retrying; offset pagination does not retain an issued frozen snapshot. Enumeration continues in one owned background stream, publishing sorted immutable batches. There is no total 200000-path, 32-MiB-output or 30-second cutoff. indexing=true means work is still running: consume the current pages, then poll from offset 0 until complete=true. indexError reports actual enumeration failures. Batch publications and refresh change generation; discard mixed page assemblies and retry. Page payloads remain bounded to 8 MiB and 20000 paths.',
     schema({'offset':{'type':'integer','minimum':0,'maximum':9223372036854775807},'limit':{'type':'integer','minimum':1,'maximum':20000},'refresh':BOOL}),
     schema({'files': {'type': 'array', 'items': STRING}, 'truncated': BOOL,'nextOffset':{'type':['integer','null']},'total':{'type':'integer'},'indexRevision':REVISION,'generation':REVISION,'indexing':BOOL,'complete':BOOL,'indexError':STRING}, ('files', 'truncated','nextOffset','total','indexRevision','generation')), []),
    ('workspace_poll','workspace.poll','Reconcile disk changes: reload clean open buffers, retain dirty/missing buffers and report conflicts.',schema(),
     schema({'revision':REVISION,'indexRevision':REVISION,'pluginsRevision':REVISION,'reloaded':{'type':'array','items':STRING},'conflicts':ARRAY,'watcher':{'type':'object'},'recovery':RECOVERY_STATUS},('revision','indexRevision','pluginsRevision','reloaded','conflicts','watcher','recovery')),[]),
    ('search_text', 'search.text', 'Search project text. Unsaved buffers override disk for literal and regex searches. Regex uses private bounded temporary snapshots (128 buffers / 64 MiB) with native ripgrep cancellation; project files stay untouched.',
     schema({'query': {'type': 'string', 'maxLength': 4096}, 'caseSensitive': BOOL, 'regex': BOOL, 'limit': {'type': 'integer', 'minimum': 1, 'maximum': 5000}}, ('query',)),
     schema({'matches': ARRAY, 'truncated': BOOL, 'timedOut': BOOL, 'unsavedRegexExcluded': BOOL, 'cancelled': BOOL}, ('matches', 'truncated')), []),
    ('recovery_status','recovery.status','Inspect per-instance checkpoint health and pending changes. Autosnapshots never save project files.',schema(),RECOVERY_STATUS,[]),
    ('recovery_list','recovery.list','List bounded crash-recovery checkpoints. Active instances cannot be restored or discarded.',schema({'limit':{'type':'integer','minimum':1,'maximum':1024}}),
     schema({'items':{'type':'array','items':RECOVERY_ITEM},'errors':ARRAY,'truncated':BOOL,'status':RECOVERY_STATUS},('items','errors','truncated','status')),[]),
    ('recovery_preview','recovery.preview','Read a bounded UTF-8 checkpoint preview and target revision/disk guards, without changing documents.',
     schema({**RECOVERY_REF,'limitBytes':{'type':'integer','minimum':1,'maximum':262144}},tuple(RECOVERY_REF)),RECOVERY_PREVIEW,[]),
    ('recovery_restore','recovery.restore','Restore into a live buffer, never directly into the project file. Rejects active sessions, dirty targets and stale revision/disk guards. A changed disk retains a save conflict. Missing files are recreated only by an explicit document save.',
     schema({**RECOVERY_REF,'expectedRevision':NONNEGATIVE,'expectedDiskState':STRING,'includePreviousText':BOOL},(*RECOVERY_REF,'expectedRevision','expectedDiskState')),
     schema({'document':DOCUMENT,'checkpointProtected':BOOL,'sourceRetained':BOOL,'warning':STRING,'previousText':STRING},('document','checkpointProtected','sourceRetained','warning')),['workspace.write']),
    ('recovery_discard','recovery.discard','Permanently discard the selected inactive checkpoint after reviewing its reference. Never deletes its original project document.',
     schema(RECOVERY_REF,tuple(RECOVERY_REF)),schema({'discarded':BOOL,'sourceRetained':BOOL},('discarded','sourceRetained')),['workspace.write']),
    ('recovery_flush','recovery.flush','Persist current dirty buffers to recovery checkpoints now, with no project-file save. Inspect errors and pending in the returned status.',schema(),RECOVERY_STATUS,['workspace.write']),
    ('documents_list', 'documents.list', 'List documents currently open in the IDE.', schema(), schema({'documents': ARRAY}, ('documents',)), []),
    ('documents_read', 'documents.read', 'Read the live document and its revision, including unsaved edits.', schema({'path': PATH}, ('path',)), DOCUMENT, []),
    ('documents_create', 'documents.create', 'Create an empty UTF-8 file inside the workspace. Refuses to overwrite an existing file.', schema({'path': PATH}, ('path',)), DOCUMENT, ['workspace.write']),
    ('documents_create_untitled','documents.createUntitled','Create an in-memory draft with an issued untitled reference, optional filename for lexical coloring and initial text. Does not create a project file or start a language server. Drafts receive guarded recovery checkpoints; use documents_save_as to persist. Limits: 64 drafts / 64 MiB total native text.',
     schema({'name':{'type':'string','minLength':1,'maxLength':128},'text':{'type':'string','maxLength':4194304}}),DOCUMENT,['workspace.write']),
    ('documents_save_as','documents.saveAs','Save the current live text into a NEW workspace file in an existing folder and rebind the same document identity. Never overwrites an existing file/open buffer; leaves the original disk file unchanged. Expected revision guards the source. Works for issued untitled references and regular files; editor Undo is retained by the desktop. Does not permit external files.',
     schema({'path':PATH,'destination':PATH,'expectedRevision':REVISION},('path','destination','expectedRevision')),
     schema({**DOCUMENT['properties'],'previousPath':PATH,'warnings':{'type':'array','items':STRING}},(*DOCUMENT['required'],'previousPath','warnings')),['workspace.write']),
    ('documents_replace', 'documents.replace', 'Replace the live buffer using expectedRevision. Does not save to disk.',
     schema({'path': PATH, 'text': {'type': 'string', 'maxLength': 4194304}, 'expectedRevision': REVISION}, ('path', 'text', 'expectedRevision')), DOCUMENT, ['workspace.write']),
    ('documents_save', 'documents.save', 'Atomically save a live buffer, preserving BOM and line endings. Rejects external disk changes.',
     schema({'path': PATH, 'expectedRevision': REVISION}, ('path', 'expectedRevision')), DOCUMENT, ['workspace.write']),
    ('documents_close','documents.close','Close a live file or issued untitled draft with an expected revision. Dirty documents require explicit discard=true, which also retires their recovery checkpoint. Creates or deletes no project file.',
     schema({'path':PATH,'expectedRevision':REVISION,'discard':BOOL},('path','expectedRevision')),schema({'closed':BOOL},('closed',)),['workspace.write']),
    ('language_configure', 'language.configure', 'Enable, restart or disable a built-in or plugin language server in this trusted workspace. language defaults to csharp. Servers may evaluate project metadata and toolchains. arguments is full executable argv without a shell; executable is a legacy single program or C# DLL and cannot combine with arguments/settings. Omitting launch options preserves the current choice or restores this workspace\'s saved launch; explicit empty options select installed defaults. save defaults to false (session only); true persists the activation choice and unresolved launch in local application preferences and requires expectedPreferencesRevision from language_preferences. Conflicts reject before changing the running server. Core/MCP inspection never enables saved servers.',
     schema({'language':{'type':'string','minLength':1,'maxLength':160},'enabled':BOOL,**LANGUAGE_LAUNCH,
             'save':BOOL,'expectedPreferencesRevision':PREFERENCES_REVISION}), LANGUAGE_STATUS, ['process.execute']),
    ('language_preferences','language.preferences','Read and refresh per-workspace language preferences from application-owned local storage. Does not enable servers or read project-supplied launch commands. revision guards saving/forgetting across IDE instances. Unknown plugin profiles remain stored until forgotten. available=false means this embedding did not supply a preferences directory; errors preserve the existing file.',
     schema(),LANGUAGE_PREFERENCES,[]),
    ('language_preferences_forget','language.preferencesForget','Forget one saved language preference with an expected revision. Does not change or stop the running server. Installed defaults apply next session; no project files are modified. Requires trusted process configuration permission.',
     schema({'language':{'type':'string','minLength':1,'maxLength':160},'expectedPreferencesRevision':PREFERENCES_REVISION},('language','expectedPreferencesRevision')),LANGUAGE_PREFERENCES,['process.execute']),
    ('language_status', 'language.status', 'Inspect all shared language servers, availability, advertised methods and published UTF-16 diagnostics. Does not enable a server.',
     schema(), LANGUAGE_STATUS, []),
    ('language_highlight','language.highlight','Read semantic color classifications from the shared language server and current live document. Requires the expected revision; rejects concurrent document changes. Positions/lengths use UTF-16; glyphStart/glyphEnd are Unicode-codepoint indexes. supported=false means lexical fallback. Data is bounded to 50,000 tokens or 4 MiB and reports truncation. Other backends require prior language_configure; inspection never enables it.',
     schema({'path':PATH,'expectedRevision':REVISION},('path','expectedRevision')),
     schema({'path':PATH,'revision':REVISION,'supported':BOOL,'generation':{'type':'integer','minimum':0},'truncated':BOOL,
             'tokens':{'type':'array','maxItems':50000,'items':schema({'line':{'type':'integer','minimum':0},'character':{'type':'integer','minimum':0},'length':{'type':'integer','minimum':1},'glyphStart':{'type':'integer','minimum':0},'glyphEnd':{'type':'integer','minimum':1},'type':STRING,'modifiers':{'type':'array','items':STRING}},('line','character','length','glyphStart','glyphEnd','type','modifiers'))}},
            ('path','revision','supported','generation','truncated','tokens')),[]),
    ('language_complete', 'language.complete', 'Request completion items from live buffers. Returns an issued sessionId and materialized LSP items (max 2000, 4 MiB). Use an item index with language_completion_resolve or language_completion_prepare before changes_apply. Sessions expire after five minutes or eviction and reject changed revisions/workspace/server context. Other backends require language_configure.',
     schema({'path':PATH,'expectedRevision':REVISION,'position':POSITION},('path','expectedRevision','position')),schema({'path':PATH,'revision':REVISION,'sessionId':STRING,'truncated':BOOL,'result':{'type':'object'}},('path','revision','sessionId','truncated','result')), []),
    ('snippets_complete','snippets.complete','Read saved project, host-configured personal and active ABI2 plugin snippet catalogues for an already-open saved document. Merge order is project, personal, then plugins by order/id; duplicates retain cycodeSnippetSource provenance and owner labels. The optional personal store is fixed by the trusted embedding, shared across this installation and absent in a default Engine; no input path is accepted. Invalid personal storage is reported for repair, never silently omitted. result.cycodeUserRevision binds exact saved bytes; cycodeUserGeneration is local refresh metadata. User/registry changes are rechecked during issuance and native preparation, alongside the saved project catalogue, source/workspace/disk guards. Already prepared concrete plans retain ordinary apply guards. Required expectedRevision/UTF-16 position; optional ordered selection has the caret at an endpoint, at most 8 KiB of source and actual literal TM_SELECTED_TEXT use. Each catalogue accepts at most 256 KiB / 256 definitions / 8 KiB bodies, 16 prefixes per definition; plugin registry is 32 catalogues / 1 MiB including inactive registrations. At most 512 combined proposals plus the ordinary completion payload bound; truncation is reported. Selection replacement equals the issued range, without automatic reindentation. No LSP query, document opening, provider callback, executable command, installation, file edit or save. Personal editing uses snippets_user_read/save; file templates, external imports and automatic server merging remain unsupported.',
     schema({'path':PATH,'expectedRevision':REVISION,'position':POSITION,'selection':RANGE},('path','expectedRevision','position')),
     schema({'path':PATH,'revision':REVISION,'sessionId':STRING,'truncated':BOOL,'result':{'type':'object'},'language':STRING,'catalogue':STRING},('path','revision','sessionId','truncated','result','language','catalogue')),[]),
    ('snippets_user_read','snippets.userRead','Read the fixed host-configured personal snippet catalogue shared by all workspaces of this installation. Never accepts a caller file path and never creates storage. available=false means the embedding did not opt into this store. Missing files yield editable empty JSON and revision absent; valid UTF-8 malformed JSON remains editable with an error, while unreadable/binary/oversized/redirected storage cannot be overwritten through this service. revision is an opaque content guard (64 lower-case hex characters or absent); generation is local observation metadata, not a cross-process version. Existing GUI/MCP/native plugin entry points share this owner.',schema(),USER_SNIPPETS,[]),
    ('snippets_user_draft','snippets.userDraft','Capture one nonempty ordered UTF-16 selection from an already-open saved workspace document at expectedRevision into a personal snippet JSON draft. Reads the live source, including unsaved changes, without changing or saving it. Refuses untitled/external/SDK references, malformed ranges, split surrogates, selections over 8 KiB and escaped bodies over 8 KiB including the final caret. The shared literal escaping preserves dollar signs, backslashes, braces and Unicode; it creates no numbered editable field. Uses the host-configured personal store only; it must be readable valid JSON and have room under 256 definitions / 256 KiB. Adds a unique Selection snippet name, prefix snippet and the source language scope; existing definition values are retained and the complete draft is pretty-printed. Returns the original catalogue snapshot plus an editable draft, name and source provenance. Rechecks source revision/generation before return. Does not create directories, locks, files, plans or processes and never saves. Review the draft and use snippets_user_save with catalogue.revision and settings.write to persist; concurrent storage changes reject that save. The draft is an immutable captured copy, not a live source link.',
     schema({'path':PATH,'expectedRevision':REVISION,'selection':RANGE},('path','expectedRevision','selection')),
     schema({'catalogue':USER_SNIPPETS,'draft':{'type':'string','maxLength':262144},'name':STRING,'path':PATH,'revision':REVISION},('catalogue','draft','name','path','revision')),[]),
    ('snippets_user_save','snippets.userSave','Validate and explicitly save one personal JSON catalogue to the fixed application-owned file, shared across workspaces. Requires settings.write, separate from workspace.write. Checks the expected opaque revision under a nonblocking retained OS lock and uses guarded atomic replacement. At most 256 KiB UTF-8, 256 definitions and the existing body/scope grammar. Does not overwrite unreadable or redirected storage; readable malformed JSON can be repaired. Other-instance changes reject without replacing the draft; a post-write confirmation failure is explicit and needs a fresh read. No workspace source file, process or task artifact is created. Pending local snippet sessions recheck the saved revision; concrete prepared plans retain ordinary apply guards.',
     schema({'text':{'type':'string','maxLength':262144},'expectedRevision':USER_SNIPPET_REVISION},('text','expectedRevision')),USER_SNIPPETS,['settings.write']),
    ('language_completion_resolve','language.completionResolve','Resolve an item issued by language_complete using the actual server. Read-only; requires a current session, item index and expectedRevision. Rejects unsupported resolve and protected insertion-field changes. Documentation is passive data; server commands are never executed.',
     schema({'sessionId':STRING,'item':{'type':'integer','minimum':0},'expectedRevision':REVISION},('sessionId','item','expectedRevision')),
     schema({'path':PATH,'revision':REVISION,'item':{'type':'object'},'resolved':BOOL},('path','revision','item','resolved')),[]),
    ('language_completion_prepare','language.completionPrepare','Prepare one atomic live-buffer transaction for an issued server completion plus additional imports, or a snippets_complete local proposal. Resolves lazy server fields when supported (resolve defaults true); local snippets never contact a server and require the saved catalogue to remain unchanged. replace defaults true; false selects an insert range. Checks UTF-16 ranges, revisions, disk baseline and overlapping edits. Handles multiline indentation, bounded numbered snippets, mirrors, nested defaults, choices, fourteen context variables including TM_SELECTED_TEXT and bounded variable regex/case transforms. Explicit local selections can span lines, must equal the issued replacement range and must be consumed by the snippet; their layout is not automatically reindented. Interactive field transforms and arbitrary commands are unsupported. snippet contains UTF-8 byte ranges in the resulting document for interactive fields; null for ordinary text. cursor is the resulting zero-based UTF-16 final caret. Does not apply or save; use changes_preview/changes_apply.',
     schema({'sessionId':STRING,'item':{'type':'integer','minimum':0},'expectedRevision':REVISION,'resolve':BOOL,'replace':BOOL},('sessionId','item','expectedRevision')),
     schema({'path':PATH,'revision':REVISION,'plan':CHANGE,'cursor':POSITION,'snippet':{'anyOf':[SNIPPET,{'type':'null'}]}},('path','revision','plan','cursor','snippet')),[]),
    ('language_code_actions','language.codeActions','List actual language-server fixes/refactorings for a live UTF-16 range and expectedRevision. Returns a bounded issued session (max 256 actions / 2 MiB, 8 sessions / 8 MiB, five-minute TTL). Includes disabled/unsupported reasons; no server commands run. Versioned diagnostics require the current LSP overlay version; unversioned latest diagnostics are advisory and their cache is invalidated on edits. Unopened literal-edit targets are synchronized before recomputing actions.',
     schema({'path':PATH,'range':RANGE,'expectedRevision':REVISION,'only':{'type':'array','maxItems':16,'items':{'type':'string','minLength':1,'maxLength':128}}},('path','range','expectedRevision')),
     schema({'path':PATH,'revision':REVISION,'sessionId':STRING,'truncated':BOOL,'actions':{'type':'array','maxItems':256,'items':schema({'index':{'type':'integer','minimum':0},'title':STRING,'kind':STRING,'preferred':BOOL,'available':BOOL,'needsResolve':BOOL,'reason':STRING},('index','title','kind','preferred','available','needsResolve','reason'))}},('path','revision','sessionId','truncated','actions')),[]),
    ('language_code_action_prepare','language.codeActionPrepare','Resolve if needed and prepare an issued code action as a shared native change plan. Read-only: does not apply or save. Text change annotations retain labels/descriptions and issued per-plan confirmation IDs. Rejects stale revisions/workspace/server context, executable commands, snippet edits, unknown annotation references, resource operations, outside-workspace targets, disk conflicts and overlapping edits. Only the pinned TypeScript server empty acknowledgement is ignored. A newly discovered resolve target is synchronized and requires a fresh action query. Use changes_preview/apply/undo; applying requires workspace.write and acknowledgements for required annotations.',
     schema({'sessionId':STRING,'item':{'type':'integer','minimum':0},'expectedRevision':REVISION,'resolve':BOOL},('sessionId','item','expectedRevision')),CHANGE,[]),
    ('language_hover','language.hover','Read bounded symbol documentation from the actual language server at a zero-based UTF-16 position. Requires the current revision and rejects concurrent edits. Contents are display-only; Markdown/HTML and links are not executed. Inspection never enables a backend; unsupported methods fail explicitly.',
     schema({'path':PATH,'expectedRevision':REVISION,'position':POSITION},('path','expectedRevision','position')),
     schema({**INFO_FIELDS,'contents':INFO_SECTIONS,'range':{'anyOf':[RANGE,{'type':'null'}]}},(*INFO_FIELDS,'contents','range')),[]),
    ('language_signature','language.signature','Read actual callable signatures, overloads, active parameter and documentation at a zero-based UTF-16 position. Signature parameter ranges startByte/endByte are UTF-8 offsets in the returned label, or -1 when unmapped. Bounded to 32 signatures, 128 parameters each and 64 KiB of text. Requires the current revision and an enabled supporting backend.',
     schema({'path':PATH,'expectedRevision':REVISION,'position':POSITION},('path','expectedRevision','position')),
     schema({**INFO_FIELDS,'signatures':{'type':'array','maxItems':32,'items':INFO_SIGNATURE},'activeSignature':{'type':'integer','minimum':-1}},(*INFO_FIELDS,'signatures','activeSignature')),[]),
    *[(operation,method,'Read actual server-provided '+label+' locations from the live UTF-16 source position. Same native document identity/revision and backend epoch required before and after the reply; unrelated document edits do not invalidate this read. Optional expectedDocumentId/expectedEpoch guard the caller snapshot. Requires the corresponding advertised server capability. Validates Location/LocationLink target/selection/origin ranges, source-origin bounds and workspace/registered SDK access. Returns deduplicated sorted positions, fullRange, optional originRange and ownership/method/position metadata. Target positions remain index observations. Bounds: 5000 rows, 16384 visited records and 4 MiB normalized locations; invalid/outside/truncated reported separately. No file edits, installation or server enablement.',
        schema(dict(LANGUAGE_NAV_INPUT),('path','expectedRevision','position')),navigation_output(method),[])
      for operation,method,label in [('language_declaration','language.declaration','declaration'),
                                    ('language_type_definition','language.typeDefinition','type definition'),
                                    ('language_implementation','language.implementation','implementation')]],
    ('language_document_highlights','language.documentHighlights','Read in-document occurrences from the actual documentHighlight provider. Kinds: Text=1 (default), Read=2, Write=3. Text highlights may be fuzzier than references; no local name inference. Requires the live source revision; optional expectedDocumentId/expectedEpoch guard the caller. Returns the same document identity/revision/backend epoch before and after the query, source position and UTF-16 ranges checked against live buffer boundaries. Sorted and deduplicated by range (Write takes precedence over Read, then Text for duplicates). Bounds: 4096 rows, 8192 visited records, 4 MiB normalized array; invalid/truncated reported. Enabled providers only, with the actual advertised capability. No source or cursor edits, installation or enablement.',
     schema({**LANGUAGE_NAV_INPUT,'limit':{'type':'integer','minimum':1,'maximum':4096}},('path','expectedRevision','position')),OCCURRENCES,[]),
    ('language_definition', 'language.definition', 'Find definitions using live buffers and actual project references. Requires the current revision and a UTF-16 position. Returns documentId/backend/epoch/method/position provenance, normalized fullRange/optional originRange and invalid/outside/truncated counts. Replies check the same live document identity/revision and backend epoch; unrelated edits do not invalidate navigation. Optional expectedDocumentId/expectedEpoch guard the snapshot. Bounds: 5000 rows, 16384 records, 4 MiB normalized locations. Target coordinates remain index observations. Outside-workspace locations are omitted by default; includeExternal permits only registered read-only SDK sources.',
     schema(dict(LANGUAGE_NAV_INPUT), ('path', 'expectedRevision', 'position')), navigation_output('language.definition'), []),
    ('language_references', 'language.references', 'Find references using shared servers and unsaved buffers. Positions/ranges are zero-based UTF-16. Same ownership, normalization and budgets as language_definition; includeDeclaration controls the actual server request. Requires the current revision; results reflect the available project index, which may still be building.',
     schema({**LANGUAGE_NAV_INPUT, 'includeDeclaration': BOOL}, ('path', 'expectedRevision', 'position')), navigation_output('language.references'), []),
    ('vcs_status', 'vcs.status', 'Inspect Git repository status.', schema(), schema({'provider': STRING, 'repository': BOOL, 'branch': STRING, 'files': ARRAY, 'message': STRING}, ('provider', 'repository', 'branch', 'files', 'message')), []),
    ('vcs_diff', 'vcs.diff', 'Read working tree or staged differences.', schema({'path': PATH, 'staged': BOOL}), PROCESS, []),
    ('vcs_log', 'vcs.log', 'Read the latest 30 Git commits affecting the workspace.', schema(), PROCESS, []),
    ('vcs_stage', 'vcs.stage', 'Stage one workspace path.', schema({'path': PATH}, ('path',)), PROCESS, ['vcs.write']),
    ('vcs_hunks','vcs.hunks','Issue bounded text hunks for a tracked file. Requires repository root and saved buffers. Does not change the index. Binary, added/deleted and mode-changing files use whole-file staging.',schema({'path':PATH,'staged':BOOL},('path',)),
     schema({'id':STRING,'path':PATH,'staged':BOOL,'header':STRING,'reason':STRING,'hunks':{'type':'array','items':schema({'index':NONNEGATIVE,'header':STRING,'patch':STRING},('index','header','patch'))}},('id','path','staged','header','reason','hunks')),[]),
    ('vcs_apply_hunks','vcs.applyHunks','Stage selected issued hunks, or reverse staged hunks back out of the index. Refuses expired previews, changed index/diff, dirty buffers and duplicate/unknown hunk indices. Working files stay untouched.',schema({'id':STRING,'hunks':{'type':'array','minItems':1,'maxItems':1000,'items':NONNEGATIVE}},('id','hunks')),PROCESS,['vcs.write']),
    ('vcs_unstage', 'vcs.unstage', 'Unstage one workspace path.', schema({'path': PATH}, ('path',)), PROCESS, ['vcs.write']),
    ('vcs_commit', 'vcs.commit', 'Commit the existing Git index, honoring repository hooks and signing settings.', schema({'message': {'type': 'string', 'minLength': 1, 'maxLength': 4096}}, ('message',)), PROCESS, ['vcs.write']),
    ('toolchains_detect', 'toolchains.detect', 'Detect installed CMake, Ninja, Make and C/C++ toolchains. Queries tool versions; does not configure or execute project code.', schema({'refresh': BOOL}),
     schema({'cmake': STRING, 'cmakeVersion': STRING, 'ninja': STRING, 'make': STRING, 'toolchains': ARRAY, 'generators': ARRAY, 'warnings': {'type': 'array', 'items': STRING}}, ('cmake', 'cmakeVersion', 'ninja', 'make', 'toolchains', 'generators', 'warnings')), []),
    ('build_inspect', 'build.inspect', 'Inspect the saved build profile and CMake File API target/toolchain metadata without executing project code.', schema(BUILD_FIELDS), BUILD_PROJECT, []),
    ('build_presets','build.presets','Read all five CMake preset families, included workspace files, inheritance, linked binary/output paths and workflow steps without executing project tools. Per-preset errors are explicit; native CMake validates conditions and complete execution semantics. Tests/packages/workflows use build_preset_plan/run; ordinary configure/build presets retain their existing profile controls.',schema({'source':PATH}),CMAKE_PRESETS,[]),
    ('build_status', 'build.status', 'Inspect the currently owned CMake, MSBuild or explicit Unreal build operation. Operation IDs correlate cancellation, not authorization.', schema(), schema({'running':BOOL,'operationId':STRING,'method':STRING,'cancelling':BOOL},('running','operationId','method','cancelling')), []),
    ('build_location','build.location','Resolve one compiler display line to an existing workspace file without executing tools or reading file contents. Supports explicit path(line,column), path:line:column, path:line, Rust arrows and GLSL ERROR/WARNING file prefixes, plus numeric MSBuild project prefixes. Coordinates are one-based reported compiler coordinates, not guaranteed Unicode character columns. text is limited natively to 16 KiB UTF-8 bytes with no NUL/newline/control bytes; directory defaults to workspace root, must already exist inside the workspace and is never guessed. Missing/external/symlink-escaping/URI paths and numeric shader source IDs are unmapped. No ANSI, tool code-page decoding, terminal, source-map or include/caret inference. A location does not establish a current diagnostic or source revision. GUI row activation uses the actual recorded live task directory when present; combined final logs use workspace root. Supplied text only; no process.execute permission needed.',COMPILER_LOCATION_INPUT,COMPILER_LOCATION,[]),
    ('build_output','build.output','Read the latest explicit native build output tail without waiting for adapter/plugin/document locks. Retains at most 256 KiB of raw merged stdout/stderr in memory until the next build or workspace close. CMake configure/build/preset execution, MSBuild build/rebuild/clean/restore and explicit Unreal UBT/UAT execution opt in; tool discovery, structured evaluation, background analysis, project tasks and trusted plugin-owned processes are excluded. Initial requests use {}; conditional requests supply both streamId and revision. changed:true replaces the entire prior display tail; changed:false has empty output. A new streamId distinguishes builds even when operationId is reused. byteCount/droppedBytes count original bytes, not returned Unicode characters; text repairs invalid UTF-8, NUL/control bytes and may replace a split character at either retained edge. No ANSI/terminal interpretation, code-page detection, history archive, progress percentage, implicit artifact or SDK/MCP push notification. incomplete reports a journal failure; lifecycle/final exit remain available through the original build response. process.execute is required because compiler output can contain private project data. Poll at about 250 ms or slower; no cancellation or mutation of the running build.',BUILD_OUTPUT_INPUT,BUILD_OUTPUT,['process.execute']),
    ('build_cancel', 'build.cancel', 'Request cancellation of an owned CMake/MSBuild/Unreal process tree. An optional operationId must match. The native build reservation stays held until execution stops; generated project outputs are retained.', schema({'operationId':{'type':'string','maxLength':128}}), schema({'requested':BOOL},('requested',)), ['process.execute']),
    ('build_configure', 'build.configure', 'Configure a trusted CMake project manually or through a configurePreset from build_presets. Presets retain native CMake cache/toolchain/environment/condition semantics; CyCode selects the guarded binaryDir and requests compilation/File API metadata. Executes project code. Saves the active profile after successful inspection. Manual mode refuses generator/compiler changes in an existing tree; CMake validates preset mode caches.', schema(BUILD_FIELDS), BUILD_RESULT, ['process.execute']),
    ('build_run', 'build.run', 'Build an already configured trusted CMake project, optionally with native buildPreset targets/jobs/environment. target explicitly overrides the target set. Uses and saves the successful profile. Presets are checked for source-root paths and changes before/after execution.', schema(BUILD_FIELDS), BUILD_RESULT, ['process.execute']),
    ('plugins_list', 'plugins.list', 'List loaded native plugins, commands and revision-control providers.', schema(),
     schema({'abi': {'type': 'integer'}, 'supportedAbis':{'type':'array','items':{'type':'integer'}}, 'revision':REVISION,
             'host':{'type':'object'},'eventSubscriptions':ARRAY,'debugAdapters':ARRAY,
             'snippetCatalogues':{'type':'array','items':schema({'id':STRING,'plugin':STRING,'title':STRING,
                 'order':{'type':'integer'},'definitionCount':{'type':'integer','minimum':0}},
                 ('id','plugin','title','order','definitionCount'))},
             'plugins': ARRAY, 'commands': ARRAY, 'knownCommands':ARRAY, 'vcsProviders': ARRAY, 'menus':ARRAY, 'panels':ARRAY, 'projects':ARRAY,'syntaxes':ARRAY,'templates':ARRAY,'languageServers':ARRAY},
            ('abi','supportedAbis','revision','plugins','commands','knownCommands','vcsProviders','menus','panels','projects','syntaxes','templates')), []),
    ('plugins_invoke', 'plugins.invoke', 'Execute a registered command or panel callback from a trusted native plugin. Native code and exported host services require editing/process permissions.', schema({'command': STRING, 'input': {'type': 'object'}}, ('command',)), {'type': 'object'}, ['plugins.execute','workspace.write','process.execute']),
    ('plugins_configure','plugins.configure','Enable or disable an installed plugin for this workspace. Checks dependencies and atomically persists settings; libraries remain pinned until host shutdown.',
     schema({'id':STRING,'enabled':BOOL,'expectedRevision':REVISION},('id','enabled','expectedRevision')),{'type':'object'},['plugins.execute','workspace.write','process.execute']),
    ('plugins_project','plugins.project','Invoke an advertised project-provider action through the shared native plugin host.',
     schema({'provider':STRING,'action':STRING,'input':{'type':'object'}},('provider','action')),{'type':'object'},['plugins.execute','workspace.write','process.execute']),
    ('vcs_providers','vcs.providers','List available version control providers and their declared capabilities/actions.',schema(),schema({'providers':ARRAY},('providers',)),[]),
    ('vcs_context','vcs.context','Resolve the version control provider for a workspace path using persisted most-specific project mappings.',
     schema({'contextPath':PATH}),{'type':'object'},[]),
    ('vcs_configure','vcs.configure','Persist a version control provider mapping for a project folder; detects concurrent configuration changes.',
     schema({'root':PATH,'provider':STRING,'expectedRevision':REVISION},('root','provider','expectedRevision')),{'type':'object'},['vcs.write','workspace.write']),
    ('vcs_provider_action','vcs.providerAction','Run an advertised action of an installed external version control provider. The built-in Git provider uses its typed operations.',
     schema({'provider':STRING,'action':STRING,'contextPath':PATH,'input':{'type':'object'}},('action',)),{'type':'object'},['plugins.execute','vcs.write','workspace.write','process.execute']),
]
METHODS = {operation[0]: operation[1] for operation in OPERATIONS}
GIT_METHODS = frozenset(method for method in METHODS.values() if method.startswith('vcs.') and
    method not in ('vcs.providers','vcs.context','vcs.configure','vcs.providerAction','vcs.repositories'))
for _id, _method, _description, _inputs, _outputs, _permissions in OPERATIONS:
    if _method=='unreal.automationPlan': _inputs['allOf']=UNREAL_AUTOMATION_RULES
    if _method in ('vcs.replay.plan','vcs.replay.apply'):
        _inputs['allOf']=GIT_REPLAY_RULES
    if _method in ('vcs.merge.plan','vcs.merge.commit','vcs.merge.quit'):
        _inputs['allOf']=GIT_MERGE_INPUT_RULES
        if _method!='vcs.merge.plan': _inputs['properties']['action']={'const':'commit' if _method=='vcs.merge.commit' else 'quit'}
    if _method in ('debug.continue','debug.next','debug.stepIn','debug.stepOut'):
        _inputs['properties'].update(DEBUG_EXECUTION_CONTEXT)
    if _method in ('debug.evaluate','debug.variables','debug.source','debug.frame','debug.thread',
                   'debug.continue','debug.next','debug.stepIn','debug.stepOut','debug.memory','debug.disassemble','debug.exceptionInfo'):
        _inputs['properties']['stopGeneration'] = {'type':'integer','minimum':0,
            'description':'Observed debug_status.stopGeneration; rejects inspection queued for another stop. Omission pins the current stop when queued.'}
    if _method in ('build.configure','build.run','build.presetRun','msbuild.build','msbuild.rebuild','msbuild.clean','msbuild.restore',
                   'unreal.generate','unreal.database','unreal.build','unreal.prepare','unreal.automationRun'):
        _inputs['properties']['timeoutSeconds'] = {'type':'integer','minimum':1,'maximum':86400,
            'description':'Deadline for the main build/generation subprocess. Default is 180 seconds (MSBuild 150); discovery/evaluation bounds remain separate. Cancellation retains process ownership.'}
        if _method=='unreal.automationRun': _inputs['properties']['timeoutSeconds']['description']='Required exact deadline from the issued automation preview (1..86400 seconds). The native owner rejects a mismatch and the bridge waits for actual process termination.'
    if _method in GIT_METHODS:
        _inputs['properties']['repository']={'type':'string','minLength':1,'description':'Workspace-relative Git folder; defaults to the workspace. Document paths remain workspace-relative.'}
        _inputs['properties']['expectedRepository']=GIT_GUARD
    if _method.startswith('vcs.') and _method!='vcs.repositories':
        _inputs['properties']['provider'] = ({'type':'string','const':'git',
            'description':'Typed Git contract. External providers use vcs_provider_action with its explicit permissions.'}
            if _method in GIT_METHODS else {'type':'string','description':'Registered VCS provider ID.'})

def write_manifest():
    path = Path(__file__).with_name('CyTool.json')
    manifest = json.loads(path.read_text('utf-8'))
    manifest.pop('standaloneExecutable',None)
    manifest.update(name='CyCode Core',description='Native C++ Core services, shared by applications and CyToolsCore MCP.',
                    executable='python app/tool.py', categories=['development'], tags=['Core', 'C++', 'plugins'],
                    capabilities=['jobs', 'cancellation', 'live-documents', 'text-search', 'cpp-navigation', 'cpp-symbols', 'cpp-refactoring', 'csharp-language', 'multilingual-lsp', 'project-tasks', 'readonly-sdk-sources', 'terminal', 'dap-debugger', 'git', 'native-plugins', 'msbuild'],
                    concurrency={'policy': 'Concurrent', 'maxWorkers': 4}, distributionStatus='Development milestone; see app/Docs/Validation.md')
    for platform in manifest['platforms']:
        platform['status'] = 'Experimental'
    manifest['operations'] = [dict(id=op, name=op.replace('_', ' ').title(),
        description=description+(' This typed operation always targets built-in Git in the selected workspace repository; use vcs_provider_action for plugins.' if method in GIT_METHODS else ''),
        inputSchema=inputs, outputSchema=outputs, resourceRequirements=({'ramBytes':64*1024*1024,**({'diskBytes':272*1024*1024} if method=='packages.install' else {})} if method.startswith('packages.') else {'ramBytes': (64 if method=='unreal.artifacts' else 256 if method.startswith(('vcs.conflict.','vcs.checkout.','vcs.merge.','vcs.replay.','unreal.automation')) else 16) * 1024 * 1024}),
        supportedBackends=[], permissions=permissions, canRunAsync=True, canCancel=not (method.startswith('packages.') or method.startswith('vcs.replay.') or method.startswith('vcs.merge.') or method.startswith('vcs.checkout.') or method.startswith('vcs.conflict.') or method.startswith('vcs.worktree.') or method.startswith('vcs.submodule') or method in DEBUG_MUTATIONS or method in DEBUG_EXECUTIONS or method in ('snippets.userSave','documents.createUntitled','documents.saveAs','documents.close','debug.dataBreakpoints','debug.runToCursor','debug.restoreRunToCursor','debug.instructionBreakpoints')), supportsProgress=method in ('analysis.prepare','language.rename'),
        **({'exclusiveGroups':['semantic-preparation']} if method in ('analysis.prepare','language.rename') else {'exclusiveGroups':['user-settings']} if method=='snippets.userSave' else {'exclusiveGroups':['plugin-packages']} if method.startswith('packages.') and method not in ('packages.list','packages.inspect') else
           {'exclusiveGroups': ['workspace-mutations']} if permissions and method not in ('terminal.list','terminal.read','debug.status','debug.profile','unreal.launchProfile','debug.cancelReload','project.cancel','project.output','tests.cancel','build.cancel','build.output','packages.list','packages.inspect') else {}))
        for op, method, description, inputs, outputs, permissions in OPERATIONS]
    path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8', newline='\n')

if __name__ == '__main__':
    write_manifest()

"""Explicit actual SDK/clangd/MSBuild acceptance; generates source data, never a test executable."""
from pathlib import Path
from contextlib import asynccontextmanager
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client

parser=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','clangd','output'):parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--run-native',action='store_true')
A=parser.parse_args()
if not A.run_native:parser.error('Explicit --run-native is required')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
files={f'Unit{i:03}.cpp':f'// échelle 🛠 {i}\n#define DOUBLE(v) ((v)*2)\ntemplate<class T> T identity(T value) {{ return value; }}\nint unit_{i}(int value) {{ return identity(DOUBLE(value)) + {i}; }}\n' for i in range(160)}
for name,text in files.items():(WORK/name).write_text(text,encoding='utf-8',newline='\n')
(WORK/'00_binary.cpp').write_bytes(b'\xff\x00not a UTF-8 text buffer\n')
(WORK/'01_oversized.cpp').write_bytes(b'// source larger than the documented live-buffer limit\n'+b' '*5*1024*1024)
names=sorted(files)+['00_binary.cpp','01_oversized.cpp']
baseline={name:hashlib.sha256((WORK/name).read_bytes()).hexdigest() for name in names}
project='''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><ProjectGuid>{53A53149-3639-447C-A64F-41679980E66E}</ProjectGuid></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props"/>
<PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling><PreprocessorDefinitions>CORE_CAMPAIGN=1;%(PreprocessorDefinitions)</PreprocessorDefinitions></ClCompile></ItemDefinitionGroup>
<ItemGroup>'''+''.join('<ClCompile Include="'+name+'"/>' for name in names)+'''</ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets"/></Project>'''
(WORK/'Campaign.vcxproj').write_text(project,encoding='utf-8')
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':{},'sourceFiles':len(names)}
def mark(name,value=True):
    report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(result):
    if result.isError:raise RuntimeError(result.content[0].text)
    return json.loads(result.content[0].text)
async def submit(s,name,args):return decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId']
async def wait(s,job,expect=None):
    until=time.monotonic()+240
    while time.monotonic()<until:
        value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
        if value['state'] in ('Completed','Failed','Cancelled'):
            if expect is not None:assert value['state']==expect,value;return value
            assert value['state']=='Completed',value
            return value['outputs']
        await asyncio.sleep(.02)
    await s.call_tool('cy_cancel_job',{'jobId':job});raise TimeoutError(job)
async def call(s,name,args,expect=None):return await wait(s,await submit(s,name,args),expect)
@asynccontextmanager
async def host(name):
    data=OUT/name
    p=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(data),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/(name+'-stderr.log')).open('w',encoding='utf-8') as log:
      async with stdio_client(p,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize();yield s
    assert not list(data.glob('native-core-*')),'Native host did not drain'
async def configure(s,database):
    await call(s,'language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','--background-index-priority=low','-j=2','--log=error','--header-insertion=never','--compile-commands-dir='+str(database.parent)]})

async def main():
    async with host('first') as s:
        tools=await s.list_tools();assert len(tools.tools)==260;mark('actualMcpDiscovery',len(tools.tools))
        model=await call(s,'msbuild_prepare',{'entry':'Campaign.vcxproj','configuration':'Release','platform':'x64','tool':'msbuild'})
        assert model['exitCode']==0,model
        database=WORK/model['project']['compilationDatabase']['path'];rawdb=database.read_bytes();dbstat=database.stat()
        assert len(json.loads(rawdb))==162;mark('actualMsbuildCommands',162)
        await configure(s,database)
        result=await call(s,'analysis_prepare',{'scope':'compilation-database','persist':True,'batchSize':5})
        checkpoint=result['checkpoint'];checkpointfile=WORK/'.cycode/analysis-progress'/(checkpoint+'.json')
        assert result['total']==162 and result['completed']==5 and len(result['checkpointRevision'])==64
        assert result['coverage']['diagnosticUnknown']==2,result
        assert all(row['revision']==0 and row['error'] for row in result['documents'][:2])
        mark('isolatedUnreadableAndOversizedSources',result['coverage'])
        result=await call(s,'analysis_prepare',{'continuation':result['continuation'],'batchSize':2})
        assert result['completed']==7 and len(json.loads(checkpointfile.read_text('utf-8'))['records'])==7
        mark('progressPersistedBeforeShutdown',7)
    async with host('second') as s:
        await configure(s,database)
        listing=await call(s,'analysis_checkpoints',{});assert any(c['checkpoint']==checkpoint and c['observed']==7 for c in listing['checkpoints'])
        mark('discoveredAfterProcessRestart')
        result=await call(s,'analysis_prepare',{'checkpoint':checkpoint,'batchSize':2})
        assert result['completed']==9 and result['historicalObserved']==7 and result['currentPassObserved']==2
        assert result['coverage']['diagnosticUnknown']==7 and result['coverage']['diagnosticClean']==2,result
        assert all(row['evidence']=='historical-checkpoint' and row['diagnostics']=='unknown' for row in result['documents'][:7])
        mark('noHistoricalCompilerProofReused',result['coverage'])
        job=await submit(s,'analysis_prepare',{'continuation':result['continuation'],'batchSize':128})
        until=time.monotonic()+30
        while True:
            state=await call(s,'analysis_status',{})
            if state['state']=='running' and state['completed']>9:break
            assert time.monotonic()<until,state
            await asyncio.sleep(.01)
        await s.call_tool('cy_cancel_job',{'jobId':job})
        await wait(s,job,expect='Cancelled')
        state=await call(s,'analysis_status',{});assert state['state']=='cancelled',state
        after_cancel=json.loads(checkpointfile.read_text('utf-8'))
        assert len(after_cancel['records'])==state['completed'] and state['completed']>9
        mark('actualCancellationSavedProgress',state['completed'])
    async with host('third') as s:
        await configure(s,database)
        result=await call(s,'analysis_prepare',{'checkpoint':checkpoint,'batchSize':64})
        while result['continuation']:
            result=await call(s,'analysis_prepare',{'continuation':result['continuation'],'batchSize':64})
        assert result['completed']==162 and len(result['documents'])==128 and result['nextOffset']==128
        page=await call(s,'analysis_results',{'session':result['session'],'offset':128,'limit':256})
        assert len(page['documents'])==34 and page['nextOffset']==162 and not page['hasMore']
        mark('all162TraversedAndLastPageAccessible',result['coverage'])
        opened=await call(s,'documents_read',{'path':'Unit050.cpp'})
        edited=await call(s,'documents_replace',{'path':'Unit050.cpp','expectedRevision':opened['revision'],'text':'// unsaved edit during campaign\n'+opened['text']})
        failed=await call(s,'analysis_prepare',{'continuation':result['session'],'batchSize':0},expect='Failed')
        assert 'document changed' in json.dumps(failed),failed
        restored=await call(s,'documents_replace',{'path':'Unit050.cpp','expectedRevision':edited['revision'],'text':opened['text']})
        mark('liveEditInvalidatesPassWithoutSourceSave')

        refreshed=await call(s,'analysis_prepare',{'checkpoint':checkpoint,'batchSize':0})
        assert refreshed['completed']==162 and refreshed['historicalObserved']==162
        assert refreshed['currentPassObserved']==0 and refreshed['coverage']['diagnosticClean']==0
        assert refreshed['coverage']['diagnosticUnknown']==162
        mark('sameProcessSnapshotRebaseRetainsNoFreshProof')

        async with host('lease-peer') as peer:
            await configure(peer,database)
            failure=await call(peer,'analysis_prepare',{'checkpoint':checkpoint,'batchSize':0},expect='Failed')
            assert 'another Core' in json.dumps(failure)
        mark('concurrentCheckpointOwnerRefused')
    checkpointbytes=checkpointfile.read_bytes()
    changed=rawdb.replace(b'CORE_CAMPAIGN=1',b'CORE_CAMPAIGN=2',1)
    assert changed!=rawdb and len(changed)==len(rawdb)
    database.write_bytes(changed);os.utime(database,ns=(dbstat.st_atime_ns,dbstat.st_mtime_ns))
    try:
      async with host('context-guard') as s:
        await configure(s,database)
        failure=await call(s,'analysis_prepare',{'checkpoint':checkpoint,'batchSize':0},expect='Failed')
        assert 'Compilation context changed' in json.dumps(failure),failure
        mark('sameSizeTimestampCompilerChangeRefused')
        database.write_bytes(rawdb);os.utime(database,ns=(dbstat.st_atime_ns,dbstat.st_mtime_ns))
        damaged=json.loads(checkpointbytes);damaged['paths'][0]='../outside.cpp';checkpointfile.write_text(json.dumps(damaged),encoding='utf-8')
        failure=await call(s,'analysis_prepare',{'checkpoint':checkpoint,'batchSize':0},expect='Failed')
        assert 'Invalid path' in json.dumps(failure),failure
        mark('malformedCheckpointPathRefused')
    finally:
        database.write_bytes(rawdb);os.utime(database,ns=(dbstat.st_atime_ns,dbstat.st_mtime_ns));checkpointfile.write_bytes(checkpointbytes)
    assert all(hashlib.sha256((WORK/name).read_bytes()).hexdigest()==digest for name,digest in baseline.items())
    mark('originalSourceBytesPreserved');mark('allHostsDrained')
    report['state']='passed';save()
try:asyncio.run(main())
except BaseException as error:
    report.update(state='failed',error=str(error));save();raise

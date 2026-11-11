"""Actual compiler navigation/cache behavior while rolling read-only analysis opens drafts."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','clangd','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');P.add_argument('--baseline',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
source='// Unicode été 🛠\nint stable_target(int input) { return input + 1; }\nint caller() { return stable_target(7); }\n'
(WORK/'Caller.cpp').write_text(source,encoding='utf-8',newline='\n')
units=['Caller.cpp']+[f'Unit{i:03}.cpp' for i in range(160)]
for index,name in enumerate(units[1:]):(WORK/name).write_text(f'template<class T> T identity(T value) {{ return value; }}\nint unit_{index}() {{ return identity({index}); }}\n',encoding='utf-8')
(WORK/'Navigation.vcxproj').write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup><PropertyGroup Label="Globals"><ProjectGuid>{887AD176-4844-4E6A-A2E4-2CB35D9AE019}</ProjectGuid></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props"/><PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/><ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling></ClCompile></ItemDefinitionGroup><ItemGroup>'''+''.join('<ClCompile Include="'+name+'"/>' for name in units)+'''</ItemGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets"/></Project>''',encoding='utf-8')
baseline={name:hashlib.sha256((WORK/name).read_bytes()).hexdigest() for name in units}
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'baseline':A.baseline,'checks':{}}
def mark(name,value=True):report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(value):
    if value.isError:raise RuntimeError(value.content[0].text)
    return json.loads(value.content[0].text)
async def main():
    data=OUT/'jobs';params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(data),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'process.execute,workspace.write','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize()
        async def submit(name,args):return decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId']
        async def wait(job,allow_failure=False):
            until=time.monotonic()+180
            while time.monotonic()<until:
                value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):
                    if allow_failure:return value
                    assert value['state']=='Completed',value;return value['outputs']
                await asyncio.sleep(.01)
            raise TimeoutError(job)
        async def call(name,args):return await wait(await submit(name,args))
        project=await call('msbuild_prepare',{'entry':'Navigation.vcxproj','configuration':'Release','platform':'x64','tool':'msbuild'})
        assert project['exitCode']==0,project;db=WORK/project['project']['compilationDatabase']['path']
        await call('language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','-j=2','--log=error','--compile-commands-dir='+str(db.parent)]})
        doc=await call('documents_read',{'path':'Caller.cpp'});await call('analysis_prepare',{'paths':['Caller.cpp']})
        before=source[:source.index('stable_target(7)')];position={'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
        request={'path':'Caller.cpp','expectedRevision':doc['revision'],'position':position,'limit':20}
        await call('language_definition',request);cached=await call('language_definition',request)
        assert cached['locations'] and cached.get('cacheHit'),cached;mark('compilerCachePrimed')
        analysed=await call('analysis_prepare',{'scope':'compilation-database','batchSize':256})
        assert analysed['coverage']['observed']==161 and analysed['coverage']['diagnosticErrors']==0,analysed
        after=await call('language_definition',request)
        preserved=bool(after.get('cacheHit',False));mark('cacheSurvivesReadOnlyTraversal',preserved)
        if A.baseline:
            assert not preserved,'Expected baseline cache invalidation was not reproduced'
        else:assert preserved,'Read-only analysis invalidated an unrelated compiler observation'
        pending=await submit('analysis_prepare',{'scope':'compilation-database','batchSize':256})
        outcomes=[]
        for index in range(16):
            stamp=time.perf_counter();value=await wait(await submit('language_definition',request),True)
            outcomes.append({'state':value['state'],'milliseconds':(time.perf_counter()-stamp)*1000,
                'cacheHit':value.get('outputs',{}).get('cacheHit',False),
                'errorCode':value.get('error',{}).get('code') if value.get('error') else None})
        await wait(pending)
        mark('actualConcurrentNavigation',outcomes)
        if not A.baseline:assert all(row['state']=='Completed' and row['cacheHit'] for row in outcomes),outcomes
        edited=await call('documents_replace',{'path':'Caller.cpp','expectedRevision':doc['revision'],'text':'// real edit\n'+source})
        await call('analysis_prepare',{'paths':['Caller.cpp']})
        query={**request,'expectedRevision':edited['revision'],'position':{'line':position['line']+1,'character':position['character']}}
        changed=await call('language_definition',query);assert changed['locations'] and not changed.get('cacheHit',False),changed
        await call('documents_replace',{'path':'Caller.cpp','expectedRevision':edited['revision'],'text':source})
        mark('actualEditStillInvalidatesCache')
    assert not list(data.glob('native-core-*'))
    assert all(hashlib.sha256((WORK/name).read_bytes()).hexdigest()==digest for name,digest in baseline.items())
    report.update(state='baseline-measured' if A.baseline else 'passed',originalSourcesPreserved=True,hostDrained=True);save()
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

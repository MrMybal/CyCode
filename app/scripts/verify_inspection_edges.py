"""Actual clang-tidy output/context guards; no generated native test executable."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser(description=__doc__)
for key in ('core','executable','clang-tidy','output'):P.add_argument('--'+key,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
(WORK/'Bulk.cpp').write_text(''.join('typedef int T'+str(i)+';\n' for i in range(2001)),encoding='utf-8')
source_hash=hashlib.sha256((WORK/'Bulk.cpp').read_bytes()).hexdigest()
(WORK/'Flow.cpp').write_text('''int *escape_stack() { int value=7; return &value; }
int null_flow(bool take) { int *value=nullptr; if(take) return *value; return 0; }
int use_after_delete() { int *value=new int(7); delete value; return *value; }
''',encoding='utf-8')
flow_hash=hashlib.sha256((WORK/'Flow.cpp').read_bytes()).hexdigest()
(WORK/'Inspection.vcxproj').write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup><PropertyGroup Label="Globals"><ProjectGuid>{887AD176-4844-4E6A-A2E4-2CB35D9AE017}</ProjectGuid></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props"/><PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/><ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard></ClCompile></ItemDefinitionGroup><ItemGroup><ClCompile Include="Bulk.cpp"/><ClCompile Include="Flow.cpp"/></ItemGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets"/></Project>''',encoding='utf-8')
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':{}}
def mark(name,value=True):report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(value):
    if value.isError:raise RuntimeError(value.content[0].text)
    return json.loads(value.content[0].text)
async def main():
    data=OUT/'jobs';params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(data),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize()
        async def call(name,args,fail=False):
            job=decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId'];deadline=time.monotonic()+180
            while time.monotonic()<deadline:
                value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Failed','Cancelled','Completed'):
                    if fail:assert value['state']=='Failed',value;return value
                    assert value['state']=='Completed',value;return value['outputs']
                await asyncio.sleep(.02)
            raise TimeoutError(name)
        prepared=await call('msbuild_prepare',{'entry':'Inspection.vcxproj','configuration':'Release','platform':'x64','tool':'msbuild'})
        assert prepared['exitCode']==0,prepared
        db=WORK/prepared['project']['compilationDatabase']['path'];original=db.read_bytes()
        doc=await call('documents_read',{'path':'Bulk.cpp'})
        flow=await call('documents_read',{'path':'Flow.cpp'})
        inspected=await call('analysis_inspect',{'path':'Flow.cpp','expectedRevision':flow['revision'],
            'compilationDatabase':str(db.relative_to(WORK)),'profile':'recommended','executable':str(A.clang_tidy),'timeoutSeconds':120})
        assert inspected['status']=='complete' and not inspected['sourceWritten'],inspected
        codes=sorted({row['code'] for row in inspected['diagnostics']})
        assert any('NullDereference' in code for code in codes),codes
        assert any('NewDelete' in code for code in codes),codes
        assert any('StackAddressEscape' in code for code in codes),codes
        mark('realFlowAndLifetimeDiagnostics',codes)
        request={'path':'Bulk.cpp','expectedRevision':doc['revision'],'compilationDatabase':str(db.relative_to(WORK)),
                 'profile':'modernize','executable':str(A.clang_tidy),'timeoutSeconds':120}
        output=await call('analysis_inspect',request)
        mark('diagnosticCapProcessObservation',{'diagnostics':len(output['diagnostics']),'status':output['status'],'exitCode':output['exitCode'],
            'truncated':output['truncated'],'outputBytes':len(output['output'].encode()),'outputTail':output['output'][-700:]})
        assert output['truncated'] and output['status']=='truncated' and len(output['diagnostics'])==2000
        assert not output['sourceWritten']
        mark('diagnosticCapReportedExplicitly')
        try:
            malformed=[{'directory':str(WORK),'file':str(WORK/'Bulk.cpp')}]
            db.write_text(json.dumps(malformed),encoding='utf-8')
            error=await call('analysis_inspect',request,fail=True)
            assert 'real compiler command' in json.dumps(error),error
            mark('missingCompilerCommandRefused')
            malformed[0]['arguments']=[];malformed[0]['command']='clang++ -c Bulk.cpp'
            db.write_text(json.dumps(malformed),encoding='utf-8')
            error=await call('analysis_inspect',request,fail=True)
            assert 'arguments are missing or invalid' in json.dumps(error),error
            mark('emptyArgumentsCannotUseCommandFallback')
        finally:db.write_bytes(original)
    assert not list(data.glob('native-core-*'));assert hashlib.sha256((WORK/'Bulk.cpp').read_bytes()).hexdigest()==source_hash
    assert hashlib.sha256((WORK/'Flow.cpp').read_bytes()).hexdigest()==flow_hash
    mark('sourcePreservedAndHostDrained');report['state']='passed';save()
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

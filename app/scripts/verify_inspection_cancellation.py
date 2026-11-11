"""Cancel an observed actual clang-tidy child; data fixture, no test executable."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
import psutil
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser();P.add_argument('--core',type=Path,required=True);P.add_argument('--executable',type=Path,required=True);P.add_argument('--clang-tidy',type=Path,required=True);P.add_argument('--output',type=Path,required=True);P.add_argument('--run-native',action='store_true')
A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
source=''.join('int cancellation_'+str(i)+'(int value) { return value * 2 + '+str(i)+'; }\n' for i in range(25000))
(WORK/'stress.cpp').write_text(source,encoding='utf-8');sha=hashlib.sha256((WORK/'stress.cpp').read_bytes()).hexdigest()
(WORK/'Stress.vcxproj').write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup><PropertyGroup Label="Globals"><ProjectGuid>{09C98698-011F-4976-9E5A-D9F435BF383E}</ProjectGuid></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props"/><PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/><ItemGroup><ClCompile Include="stress.cpp"/></ItemGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets"/></Project>''',encoding='utf-8')
report={'state':'running','sourceSha256':sha,'sourceBytes':len(source.encode()),'sourceFunctions':25000}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(result):
    if result.isError:raise RuntimeError(result.content[0].text)
    return json.loads(result.content[0].text)
async def main():
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as session:
        await session.initialize()
        async def submit(name,args):return decode(await session.call_tool('op_'+name,{'parameters':args}))['jobId']
        async def wait(job):
            until=time.monotonic()+90
            while time.monotonic()<until:
                value=decode(await session.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):return value
                await asyncio.sleep(.02)
            raise TimeoutError('job')
        result=await wait(await submit('msbuild_prepare',{'entry':'Stress.vcxproj','configuration':'Debug','platform':'x64','tool':'msbuild'}));assert result['state']=='Completed',result
        db=result['outputs']['project']['compilationDatabase']['path']
        result=await wait(await submit('documents_read',{'path':'stress.cpp'}));assert result['state']=='Completed',result
        job=await submit('analysis_inspect',{'path':'stress.cpp','expectedRevision':result['outputs']['revision'],'compilationDatabase':db,'executable':str(A.clang_tidy),'timeoutSeconds':120})
        until=time.monotonic()+20;child=None
        while time.monotonic()<until:
            candidates=psutil.Process().children(recursive=True)
            for process in candidates:
                try:
                    if process.name().lower() in ('clang-tidy.exe','clang-tidy'):child=process;identity=child.create_time();break
                except psutil.Error:pass
            if child:break
            await asyncio.sleep(.01)
        assert child,'No actual owned analyzer child observed before cancellation'
        requested=time.monotonic();decode(await session.call_tool('cy_cancel_job',{'jobId':job}))
        result=await wait(job);assert result['state']=='Cancelled',result
        until=time.monotonic()+15
        while time.monotonic()<until:
            try:alive=child.is_running() and child.create_time()==identity
            except psutil.Error:alive=False
            if not alive:break
            await asyncio.sleep(.05)
        assert not alive,'Cancelled SDK job left its actual analyzer alive'
        state=await wait(await submit('build_status',{}));assert state['state']=='Completed' and not state['outputs']['running'],state
        report.update(actualAnalyzerChildObserved=True,cancelledJobState=result['state'],ownedChildTerminated=True,cancellationSeconds=time.monotonic()-requested,buildOwnerDrained=True)
    assert hashlib.sha256((WORK/'stress.cpp').read_bytes()).hexdigest()==sha
    assert not list((OUT/'jobs').glob('native-core-*'));report.update(state='passed',sourcePreserved=True,hostDrained=True);save();print(json.dumps(report,indent=2))
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

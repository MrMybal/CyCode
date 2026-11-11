"""Real official MCP acceptance; sources only, no generated test executable."""
from pathlib import Path
import argparse,asyncio,hashlib,json,math,os,stat,statistics,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client

P=argparse.ArgumentParser();P.add_argument('--core',type=Path,required=True);P.add_argument('--executable',type=Path,required=True);P.add_argument('--output',type=Path,required=True)
P.add_argument('--run-native',action='store_true')
P.add_argument('--clangd',type=Path,required=True)
P.add_argument('--clang-tidy',type=Path,required=True)
A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; no Core is launched by default')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
sources={'model.hpp':'''#pragma once
#ifndef QUALITY_MODE
#error Missing evaluated compilation context
#endif
#define DOUBLE_VALUE(value) ((value) * 2)
int compute(int input);
struct Base { virtual int value() const = 0; virtual ~Base() = default; };
struct Derived : Base { int value() const override { return 42; } };
template<class T> T identity(T value) { return value; }
''','main.cpp':'''#include "model.hpp"
// Unicode : été 🛠
int compute(int input) {
    int doubled = input * 2;
    return doubled + 1;
}
int uninitialized_value() { int value; return value; }
int macro_value(int input) { return DOUBLE_VALUE(input); }
int virtual_value() { Derived object; return object.value(); }
int template_value() { return identity(5); }
''','caller.cpp':'''#include "model.hpp"
int caller() { return compute(5); }
'''}
for name,text in sources.items():(WORK/name).write_text(text,encoding='utf-8',newline='\n')
project='''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
 <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
 <PropertyGroup Label="Globals"><ProjectGuid>{6AA17C1D-C03A-4AA3-8B23-1DAA239E2311}</ProjectGuid><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
 <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
 <PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup>
 <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
 <PropertyGroup><QualityMode Condition="'$(Configuration)'=='Debug'">1</QualityMode><QualityMode Condition="'$(Configuration)'=='Release'">2</QualityMode></PropertyGroup>
 <ItemDefinitionGroup><ClCompile><PreprocessorDefinitions>QUALITY_MODE=$(QualityMode);%(PreprocessorDefinitions)</PreprocessorDefinitions><LanguageStandard>stdcpp20</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions></ClCompile></ItemDefinitionGroup>
 <ItemGroup><ClCompile Include="main.cpp"/><ClCompile Include="caller.cpp"/><ClInclude Include="model.hpp"/></ItemGroup>
 <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
'''
(WORK/'Quality.vcxproj').write_text(project,encoding='utf-8')
(WORK/'Quality.sln').write_text('''Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
Project("{BC8A1FFA-BEE3-4634-8014-F334798102B3}") = "Quality", "Quality.vcxproj", "{6AA17C1D-C03A-4AA3-8B23-1DAA239E2311}"
EndProject
Global
 GlobalSection(SolutionConfigurationPlatforms) = preSolution
  Debug|x64 = Debug|x64
  Release|x64 = Release|x64
 EndGlobalSection
 GlobalSection(ProjectConfigurationPlatforms) = postSolution
  {6AA17C1D-C03A-4AA3-8B23-1DAA239E2311}.Debug|x64.ActiveCfg = Debug|x64
  {6AA17C1D-C03A-4AA3-8B23-1DAA239E2311}.Debug|x64.Build.0 = Debug|x64
  {6AA17C1D-C03A-4AA3-8B23-1DAA239E2311}.Release|x64.ActiveCfg = Release|x64
  {6AA17C1D-C03A-4AA3-8B23-1DAA239E2311}.Release|x64.Build.0 = Release|x64
 EndGlobalSection
EndGlobal
''',encoding='utf-8')
baseline={name:hashlib.sha256((WORK/name).read_bytes()).hexdigest() for name in sources}
report={'state':'running','checks':{},'sourceSha256':baseline,'nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest()}
def mark(name,value=True):
    report['checks'][name]=value;(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({'stage':name,'value':value}),flush=True)
def decode(result):
    if result.isError:raise RuntimeError('MCP validation error: '+result.content[0].text[:1500])
    data=json.loads(result.content[0].text)
    if result.isError:raise RuntimeError('MCP error: '+json.dumps(data))
    return data
def pos(text,needle,end=False):
    at=text.index(needle)+(len(needle) if end else 0);before=text[:at]
    return {'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
async def main():
    env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'}
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env=env)
    with (OUT/'mcp-stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as session:
        await session.initialize()
        tools=await session.list_tools();assert any(t.name=='op_analysis_inspect' for t in tools.tools);mark('officialSdkDiscovery',len(tools.tools))
        async def invoke(name,arguments,fail=False,timeout=240):
            job=decode(await session.call_tool('op_'+name,{'parameters':arguments}))['jobId'];until=time.monotonic()+timeout
            while time.monotonic()<until:
                value=decode(await session.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):
                    if fail:assert value['state']=='Failed',value;return value
                    if value['state']!='Completed':raise RuntimeError(name+': '+json.dumps(value))
                    return value['outputs']
                await asyncio.sleep(.01)
            decode(await session.call_tool('cy_cancel_job',{'jobId':job}));raise TimeoutError(name)
        prepared=await invoke('msbuild_prepare',{'entry':'Quality.vcxproj','configuration':'Debug','platform':'x64','tool':'msbuild'})
        assert prepared['exitCode']==0,prepared;db=prepared['project']['compilationDatabase'];assert db['entries']==2;mark('realMsBuildCommands',db['entries'])
        await invoke('language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','--use-dirty-headers','--clang-tidy','--log=error','--header-insertion=never']})
        analysis=await invoke('analysis_prepare',{'paths':list(sources)})
        assert analysis['coverage']['diagnosticErrors']==0,analysis;mark('analysisCoverage',analysis['coverage'])
        doc=await invoke('documents_read',{'path':'caller.cpp'});query={'path':'caller.cpp','expectedRevision':doc['revision'],'position':pos(doc['text'],'compute'),'limit':50}
        locations=await invoke('language_definition',query);assert locations['locations'];mark('compilerDefinition')
        references=await invoke('language_references',query);assert len(references['locations'])>=3,references;mark('compilerReferences',len(references['locations']))
        # Entire SDK job round trip, including polling; not a pure engine/cache benchmark.
        times=[]
        for _ in range(12):
            started=time.perf_counter();await invoke('language_definition',query);times.append((time.perf_counter()-started)*1000)
        mark('mcpWarmNavigation',{'samplesMilliseconds':times,'medianMilliseconds':statistics.median(times),'p95Milliseconds':sorted(times)[math.ceil(len(times)*.95)-1],'scope':'SDK/MCP round trip; OS caches not controlled'})
        plan=await invoke('language_rename',{'path':query['path'],'expectedRevision':query['expectedRevision'],'position':query['position'],'newName':'compute_renamed'})
        assert len(plan['files'])==3,plan
        await invoke('changes_apply',{'id':plan['id']});await invoke('changes_undo',{'id':plan['id']});mark('threeFileRenameApplyUndo')
        doc=await invoke('documents_read',{'path':'main.cpp'});selection={'start':pos(doc['text'],'doubled + 1'),'end':pos(doc['text'],'doubled + 1',True)}
        actions=await invoke('language_code_actions',{'path':'main.cpp','expectedRevision':doc['revision'],'range':selection,'only':['refactor']})
        (OUT/'cpp-actions.json').write_text(json.dumps(actions,indent=2)+'\n',encoding='utf-8')
        matches=[a for a in actions['actions'] if a['available'] and ('extract' in a['title'].lower())]
        assert matches,'No actual extraction offered: '+json.dumps(actions)
        action=matches[0]
        plan=await invoke('language_code_action_prepare',{'sessionId':actions['sessionId'],'item':action['index'],'expectedRevision':doc['revision']})
        preview=await invoke('changes_preview',{'id':plan['id'],'path':'main.cpp'})
        assert preview['before']!=preview['after'];assert (WORK/'main.cpp').read_text('utf-8')==sources['main.cpp']
        await invoke('changes_apply',{'id':plan['id']});changed=await invoke('documents_read',{'path':'main.cpp'});assert changed['text']!=sources['main.cpp']
        await invoke('changes_undo',{'id':plan['id']});restored=await invoke('documents_read',{'path':'main.cpp'});assert restored['text']==sources['main.cpp'];mark('clangdExtractionPreviewApplyUndo',action['title'])
        # More real clangd transformations go through the same guarded transaction.
        for label,needle,phrase in [('function','int doubled = input * 2;\n    return doubled + 1;','function'),('macro','DOUBLE_VALUE(input)','expand macro')]:
            doc=await invoke('documents_read',{'path':'main.cpp'})
            action_range={'start':pos(doc['text'],needle),'end':pos(doc['text'],needle,True)}
            actions=await invoke('language_code_actions',{'path':'main.cpp','expectedRevision':doc['revision'],'range':action_range,'only':['refactor']})
            options=[a for a in actions['actions'] if a['available'] and phrase in a['title'].lower()]
            assert options,'Expected real '+label+' transformation: '+json.dumps(actions)
            plan=await invoke('language_code_action_prepare',{'sessionId':actions['sessionId'],'item':options[0]['index'],'expectedRevision':doc['revision']})
            preview=await invoke('changes_preview',{'id':plan['id'],'path':'main.cpp'});assert preview['before']!=preview['after']
            await invoke('changes_apply',{'id':plan['id']});await invoke('changes_undo',{'id':plan['id']})
            assert (await invoke('documents_read',{'path':'main.cpp'}))['text']==sources['main.cpp']
            mark(label+'PreviewApplyUndo',options[0]['title'])
        # A valid unsaved header edit invalidates navigation observations.
        header=await invoke('documents_read',{'path':'model.hpp'})
        modified=await invoke('documents_replace',{'path':'model.hpp','expectedRevision':header['revision'],'text':'// live header edit\n'+header['text']})
        await invoke('analysis_prepare',{'paths':['model.hpp','caller.cpp']})
        caller=await invoke('documents_read',{'path':'caller.cpp'})
        moved=await invoke('language_definition',{'path':'caller.cpp','expectedRevision':caller['revision'],'position':pos(caller['text'],'compute'),'limit':20})
        assert moved['locations'] and not moved.get('cacheHit',False)
        await invoke('documents_replace',{'path':'model.hpp','expectedRevision':modified['revision'],'text':header['text']});mark('headerEditInvalidatesNavigation')
        # The native build owner reloads the actual Release compilation context.
        released=await invoke('msbuild_prepare',{'entry':'Quality.vcxproj','configuration':'Release','platform':'x64','tool':'msbuild'})
        assert released['exitCode']==0;db=released['project']['compilationDatabase']
        commands=json.loads((WORK/db['path']).read_text('utf-8'));assert all('/DQUALITY_MODE=2' in unit['arguments'] for unit in commands)
        observed=await invoke('analysis_prepare',{'paths':list(sources)});assert observed['coverage']['diagnosticErrors']==0
        mark('actualDebugToReleaseContext',{'entries':len(commands),'macro':'QUALITY_MODE=2','diagnosticErrors':0})
        restored=await invoke('documents_read',{'path':'main.cpp'})
        inspection=await invoke('analysis_inspect',{'path':'main.cpp','expectedRevision':restored['revision'],'compilationDatabase':db['path'],'executable':str(A.clang_tidy),'profile':'recommended','timeoutSeconds':120})
        (OUT/'inspection.json').write_text(json.dumps(inspection,indent=2)+'\n',encoding='utf-8')
        assert inspection['status']=='complete' and not inspection['stale'],inspection
        assert any('uninitialized' in d['code'].lower() or 'uninitialized' in d['message'].lower() for d in inspection['diagnostics']),inspection
        mark('realStaticAnalyzer',{'diagnostics':len(inspection['diagnostics']),'codes':sorted({d['code'] for d in inspection['diagnostics']})})
        await invoke('analysis_inspect',{'path':'main.cpp','expectedRevision':restored['revision']-1,'compilationDatabase':db['path'],'executable':str(A.clang_tidy)},fail=True);mark('staleInspectionRevisionRejected')
        for cycle in range(8):
            document=await invoke('documents_read',{'path':'scratch.cpp'}) if cycle else await invoke('documents_create',{'path':'scratch.cpp'})
            document=await invoke('documents_replace',{'path':'scratch.cpp','expectedRevision':document['revision'],'text':'// owned save cycle\nint saved = '+str(cycle)+';\n'})
            await invoke('recovery_flush',{});await invoke('documents_save',{'path':'scratch.cpp','expectedRevision':document['revision']})
        assert not list(WORK.glob('.cycode-save-*.tmp'));mark('sharedAtomicSaves',8)
        scratch=await invoke('documents_read',{'path':'scratch.cpp'});saved=(WORK/'scratch.cpp').read_bytes()
        os.chmod(WORK/'scratch.cpp',stat.S_IREAD)
        try:
            scratch=await invoke('documents_replace',{'path':'scratch.cpp','expectedRevision':scratch['revision'],'text':'// read-only attempt\n'})
            await invoke('documents_save',{'path':'scratch.cpp','expectedRevision':scratch['revision']},fail=True)
            assert (WORK/'scratch.cpp').read_bytes()==saved;mark('readOnlySavePreservedDisk')
        finally:os.chmod(WORK/'scratch.cpp',stat.S_IREAD|stat.S_IWRITE)
        scratch=await invoke('documents_save',{'path':'scratch.cpp','expectedRevision':scratch['revision']})
        external=b'// external content must survive\n';(WORK/'scratch.cpp').write_bytes(external)
        scratch=await invoke('documents_replace',{'path':'scratch.cpp','expectedRevision':scratch['revision'],'text':'// conflicting buffer\n'})
        await invoke('documents_save',{'path':'scratch.cpp','expectedRevision':scratch['revision']},fail=True)
        assert (WORK/'scratch.cpp').read_bytes()==external;mark('diskConflictPreservedExternalContent')

        assert all(hashlib.sha256((WORK/n).read_bytes()).hexdigest()==digest for n,digest in baseline.items());mark('originalSourcesPreserved')
    assert not list((OUT/'jobs').glob('native-core-*'));mark('hostDrained')
    report['state']='passed';(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
try:asyncio.run(main())
except BaseException as error:
    report.update(state='failed',error=str(error));(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise

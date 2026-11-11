"""Actual MSBuild/clangd template, macro-argument and configuration acceptance."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','clangd','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; creates source data, never a fixture executable')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
sources={'model.hpp':'''#pragma once
#define CY_DOUBLE(value) ((value)+(value))
namespace demo {
#if QUALITY_MODE == 1
using Active = int;
#else
using Active = long long;
#endif
template<class T> struct Box { T payload; T read() const { return payload; } };
template<class T> T pass(T value) { return value; }
}
''','main.cpp':'''#include "model.hpp"
demo::Active compute() {
    demo::Box<demo::Active> item{3};
    return demo::pass(item.read()) + CY_DOUBLE(item.payload);
}
''','caller.cpp':'''#include "model.hpp"
demo::Active other() {
    demo::Box<demo::Active> item{7};
    return demo::pass(item.payload);
}
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
</Project>'''
(WORK/'Templates.vcxproj').write_text(project,encoding='utf-8')
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':{}}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def mark(n,v=True):report['checks'][n]=v;save();print(json.dumps({'stage':n,'value':v}),flush=True)
def decode(r):
    if r.isError:raise RuntimeError(r.content[0].text)
    return json.loads(r.content[0].text)
def position(text,needle):
    before=text[:text.index(needle)];return {'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
async def main():
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize()
        async def call(name,args):
            job=decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId'];until=time.monotonic()+240
            while time.monotonic()<until:
                value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):
                    assert value['state']=='Completed',(name,value);return value['outputs']
                await asyncio.sleep(.02)
            raise TimeoutError(name)
        for config,expected in [('Debug','int'),('Release','long long')]:
            prepared=await call('msbuild_prepare',{'entry':'Templates.vcxproj','configuration':config,'platform':'x64','tool':'msbuild'});assert prepared['exitCode']==0
            await call('language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','--use-dirty-headers','--log=error','--header-insertion=never']})
            result=await call('analysis_prepare',{'paths':list(sources)});assert result['coverage']['diagnosticErrors']==0,result
            doc=await call('documents_read',{'path':'main.cpp'})
            base={'path':doc['path'],'expectedRevision':doc['revision'],'position':position(doc['text'],'read()')}
            definition=await call('language_definition',base);assert any(r['path']=='model.hpp' for r in definition['locations']),definition
            hover=await call('language_hover',{**base,'position':position(doc['text'],'Active')});assert expected in json.dumps(hover),hover
            header=await call('documents_read',{'path':'model.hpp'})
            plan=await call('language_rename',{'path':'model.hpp','expectedRevision':header['revision'],'position':position(header['text'],'payload'),'newName':'storedValue'})
            assert {r['path'] for r in plan['files']}==set(sources),plan
            await call('changes_apply',{'id':plan['id']})
            for name in sources:
                changed=await call('documents_read',{'path':name});assert 'payload' not in changed['text'] and 'storedValue' in changed['text']
            await call('changes_undo',{'id':plan['id']})
            for name,text in sources.items():assert (await call('documents_read',{'path':name}))['text']==text and (WORK/name).read_text('utf-8')==text
            mark(config,{'templateMemberDefinition':True,'evaluatedAlias':expected,'threeFileTemplateFieldAndMacroArgumentRenameApplyUndo':True,'sourcePreserved':True})
    assert not list((OUT/'jobs').glob('native-core-*'));report['state']='passed';mark('hostDrained')
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

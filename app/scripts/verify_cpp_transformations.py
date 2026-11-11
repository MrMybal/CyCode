"""Exercise actual compiler transformations through existing guarded MCP plans."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','clangd','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
sources={'Tweaks.h':'''#pragma once
// Types : été 🛠
#define DOUBLE_VALUE(value) ((value)*2)
namespace tools {
struct Widget {
    int outlined(int input) { return input + 1; }
    int inlineLater(int input);
};
}
''','Tweaks.cpp':'''#include "Tweaks.h"
using namespace tools;
int tools::Widget::inlineLater(int input) { return input * 3; }
int local_actions(int input) {
    auto local = input + 1;
    int inline_value = input * 3;
    if (input > 0) { return inline_value; } else { return local; }
}
int macro_value(int input) { return DOUBLE_VALUE(input); }
int extract_variable(int input) { return (input * 4) + 1; }
int extract_function(int input) {
    int value = input * 2;
    value += 3;
    return value;
}
namespace conversion {
enum Shade { Dark, Light };
Shade choose() { return Dark; }
}
int using_type(Widget &widget) { return widget.outlined(2); }
'''}
for name,text in sources.items():(WORK/name).write_text(text,encoding='utf-8',newline='\n')
(WORK/'Tweaks.vcxproj').write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup><PropertyGroup Label="Globals"><ProjectGuid>{887AD176-4844-4E6A-A2E4-2CB35D9AE018}</ProjectGuid></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props"/><PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/><ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling></ClCompile></ItemDefinitionGroup><ItemGroup><ClCompile Include="Tweaks.cpp"/><ClInclude Include="Tweaks.h"/></ItemGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets"/></Project>''',encoding='utf-8')
baseline={name:hashlib.sha256((WORK/name).read_bytes()).hexdigest() for name in sources}
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'transformations':[]}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(value):
    if value.isError:raise RuntimeError(value.content[0].text)
    return json.loads(value.content[0].text)
def position(text,index):
    before=text[:index];return {'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
async def main():
    data=OUT/'jobs';params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(data),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'process.execute,workspace.write','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize()
        async def call(name,args,fail=False):
            job=decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId'];deadline=time.monotonic()+180
            while time.monotonic()<deadline:
                value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Failed','Cancelled','Completed'):
                    if fail:return value
                    assert value['state']=='Completed',value;return value['outputs']
                await asyncio.sleep(.02)
            raise TimeoutError(name)
        prepared=await call('msbuild_prepare',{'entry':'Tweaks.vcxproj','configuration':'Release','platform':'x64','tool':'msbuild'})
        assert prepared['exitCode']==0,prepared;db=WORK/prepared['project']['compilationDatabase']['path']
        await call('language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','-j=2','--log=error','--compile-commands-dir='+str(db.parent)]})
        cases=[('InlineVariable','Tweaks.cpp','inline_value',['inline variable']),
            ('ExpandDeducedType','Tweaks.cpp','auto',['auto','deduced type']),
            ('DefineInline','Tweaks.cpp','inlineLater',['inline','declaration']),
            ('DefineOutline','Tweaks.h','outlined',['out-of-line','outline']),
            ('RemoveUsingNamespace','Tweaks.cpp','using namespace tools;',['remove using']),
            ('ScopifyEnum','Tweaks.cpp','Shade',['scoped enum','enum class']),
            ('SwapIfBranches','Tweaks.cpp','if (input > 0)',['swap']),
            ('ExtractVariable','Tweaks.cpp','input * 4',['extract subexpression']),
            ('ExtractFunction','Tweaks.cpp','    int value = input * 2;\n    value += 3;\n    return value;',['extract to function']),
            ('ExpandMacro','Tweaks.cpp','DOUBLE_VALUE(input)',['expand macro'])]
        for identity,name,needle,labels in cases:
            for path in sources:await call('documents_read',{'path':path})
            await call('analysis_prepare',{'paths':list(sources)})
            doc=await call('documents_read',{'path':name});text=doc['text'];index=text.index(needle)
            selection={'start':position(text,index),'end':position(text,index+len(needle))}
            if identity in ('InlineVariable','DefineInline','DefineOutline','ScopifyEnum','SwapIfBranches'):
                selection['end']=selection['start']
            listed=await call('language_code_actions',{'path':name,'expectedRevision':doc['revision'],'range':selection})
            candidates=[row for row in listed['actions'] if any(label in row['title'].lower() for label in labels)]
            row={'id':identity,'offered':[item['title'] for item in listed['actions']],'state':'not-offered'}
            available=[item for item in candidates if item['available']]
            if available:
                selected=available[0];row['title']=selected['title']
                outcome=await call('language_code_action_prepare',{'sessionId':listed['sessionId'],'item':selected['index'],'expectedRevision':doc['revision']},fail=True)
                if outcome['state']=='Completed':
                    plan=outcome['outputs'];assert plan['files'],plan
                    await call('changes_apply',{'id':plan['id']})
                    changed=False
                    for path,original in sources.items():
                        changed=changed or (await call('documents_read',{'path':path}))['text']!=original
                    assert changed,'Compiler preview applied no source changes'
                    await call('changes_undo',{'id':plan['id']})
                    for path,original in sources.items():assert (await call('documents_read',{'path':path}))['text']==original
                    row.update(state='preview-apply-undo-passed',files=len(plan['files']))
                else:row.update(state='preparation-refused',reason=outcome.get('error'))
            elif candidates:row.update(state='unavailable',reasons=[item['reason'] for item in candidates])
            report['transformations'].append(row);save();print(json.dumps(row),flush=True)
    assert not list(data.glob('native-core-*'))
    assert all(hashlib.sha256((WORK/name).read_bytes()).hexdigest()==digest for name,digest in baseline.items())
    report.update(state='passed',originalSourcesPreserved=True,hostDrained=True,
        validated=sum(row['state']=='preview-apply-undo-passed' for row in report['transformations']))
    required={'ExpandDeducedType','DefineInline','DefineOutline','RemoveUsingNamespace','ScopifyEnum','ExtractVariable','ExtractFunction','ExpandMacro'}
    passed={row['id'] for row in report['transformations'] if row['state']=='preview-apply-undo-passed'}
    assert required <= passed, 'Required compiler transformations failed: '+str(sorted(required-passed))
    save()
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

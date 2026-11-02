"""Exercise actual solution/.NET builds and C++ indexing through native GUI keys."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
CLI=ROOT/'runtime/build-vs/Release/cycode_cli.exe'
EXE=ROOT/'CyCode.exe'
REPORTS=ROOT/'data/reports'
REPORTS.mkdir(parents=True,exist_ok=True)
def native(root,method,params=None):
    result=subprocess.run([str(CLI),str(root),method,json.dumps(params or {})],check=True,capture_output=True,timeout=180)
    value=json.loads(result.stdout)
    if value.get('exitCode',0): raise RuntimeError(value['output'])
    return value
def chord(frame,*keys): return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]
def action(frame,text): return chord(frame,'Ctrl','Shift','A')+[{'frame':frame+12,'keys':[],'text':text}]+chord(frame+28,'Enter')
def run(root,name,events=(),theme='graphite',language='en',opened='src/main.cpp',panel='msbuild',profile='rider',navigation=None,allow_error=False):
    report=REPORTS/f'msbuild-{name}.json'
    args=[str(EXE),'--workspace',str(root),'--open',opened,'--theme',theme,'--language',language,'--keymap',profile,
        '--test-frames','190','--test-wait-idle','--test-keys',json.dumps(events),'--test-report',str(report),
        '--test-capture',str(REPORTS/f'msbuild-{name}.bmp'),'--capture-panel',panel]
    if navigation: args+=['--test-navigation',json.dumps(navigation)]
    subprocess.run(args,check=True,capture_output=True,timeout=65)
    state=json.loads(report.read_text('utf8'))
    assert state['msbuildActive'],state
    if not allow_error: assert state['error']=='',state
    print('MSBuild GUI '+name+': passed',flush=True)
    return state
with tempfile.TemporaryDirectory(prefix='msbuild-ui-é-',dir=ROOT/'data/validation') as folder:
    root=Path(folder);src=root/'src';src.mkdir();(src/'include').mkdir()
    source='#include "widget.hpp"\nint main() { Widget w; return w.Value == 42 ? 0 : 1; }\n'
    (src/'main.cpp').write_text(source,'utf8')
    (src/'include/widget.hpp').write_text('#pragma once\nstruct Widget { int Value = MSBUILD_VALUE; };\n','utf8')
    (src/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(GuiSolution LANGUAGES CXX)\nadd_executable(GuiNative main.cpp)\ntarget_include_directories(GuiNative PRIVATE include)\ntarget_compile_definitions(GuiNative PRIVATE MSBUILD_VALUE=42)\n','utf8')
    cmake=native(root,'toolchains.detect')['cmake']
    subprocess.run([cmake,'-S',str(src),'-B',str(root),'-G','Visual Studio 17 2022','-A','x64'],check=True,capture_output=True,timeout=50)
    solution=root/'GuiSolution.sln'
    # Build must save the live source, using revision/disk guards, before invoking MSBuild.
    # The editor inserts the closing brace automatically while typing.
    edited='int main() { return 17;'
    events=chord(20,'Ctrl','A')+[{'frame':30,'keys':[],'text':edited}]+chord(55,'Ctrl','F9')
    state=run(solution,'build',events)
    assert 'build.run' in state['ui']['commands'] and (root/'Debug/GuiNative.exe').exists(),state
    assert subprocess.run([str(root/'Debug/GuiNative.exe')]).returncode==17,'Build used stale disk source'
    assert ''.join((src/'main.cpp').read_text('utf8').split())=='intmain(){return17;}','Build did not save the live source'
    (src/'main.cpp').write_text(source,'utf8')
    state=run(solution,'prepare',action(35,'Prepare MSBuild code analysis'))
    assert state['msbuild']['compilationDatabase']['available'],state['msbuild']
    state=run(solution,'definition',navigation={'path':'src/main.cpp','position':{'line':1,'character':13}},panel='references')
    assert state['active']=='src/include/widget.hpp',state
    state=run(solution,'evaluate',action(35,'Evaluate MSBuild project'))
    assert state['msbuild']['artifacts'] and state['msbuild']['artifacts'][0]['path'],state['msbuild']
    run(solution,'evaluate-repeat',action(35,'Evaluate MSBuild project'))
    state=run(solution,'light-fr',action(35,'MSBuild'),theme='light-grey',language='fr')
    assert 'view.msbuild' in state['ui']['commands'],state
    run(solution,'clean',action(35,'Clean MSBuild project'))
    assert not (root/'Debug/GuiNative.exe').exists(),'Clean shortcut did not run MSBuild'
    run(solution,'rebuild',action(35,'Rebuild MSBuild project'))
    assert (root/'Debug/GuiNative.exe').exists()
    assert (src/'main.cpp').read_text('utf8')==source,'GUI unexpectedly saved source'
    (src/'main.cpp').write_text('int main() { UnknownCompileValue; }\n','utf8')
    failed=run(solution,'compiler-error',chord(35,'Ctrl','F9'),panel='problems',allow_error=True)
    assert failed['error'] and any(d['path']=='src/main.cpp' and d['severity']=='error' for d in failed['buildDiagnostics']),failed
    managed=root/'managed';managed.mkdir()
    (managed/'GuiManaged.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework><OutputType>Exe</OutputType></PropertyGroup></Project>','utf8')
    (managed/'Program.cs').write_text('System.Console.WriteLine("GUI_NET_42");\n','utf8')
    (managed/'NuGet.Config').write_text('<configuration><packageSources><clear /></packageSources></configuration>','utf8')
    native(managed/'GuiManaged.csproj','msbuild.restore',{'tool':'dotnet'})
    state=run(managed/'GuiManaged.csproj','managed',chord(35,'Ctrl','Shift','B'),opened='Program.cs',profile='visualstudio')
    assert (managed/'bin/Debug/net8.0/GuiManaged.dll').exists(),state
    dotnet=next(t['executable'] for t in state['msbuild']['tools'] if t['id']=='dotnet')
    assert 'GUI_NET_42' in subprocess.check_output([dotnet,str(managed/'bin/Debug/net8.0/GuiManaged.dll')]).decode('utf8')
print('Native GUI: MSBuild build/clean/rebuild, evaluated C++ navigation, diagnostics, themes and .NET build passed.')

"""Real IDE + clangd color pipeline; all OS windows remain hidden during capture."""
import json
import os
import argparse
import sys
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
SOURCE='''#include "types.hpp"
#define TRACE_COUNT 42

// Aliases get their actual type color from clangd.
using LocalAlias = Widget;

int main()
{
    const char* Unicode = "🛠";
    LocalAlias Counter{};
    int FValue = TRACE_COUNT; // This is a variable, not an Unreal type.
\tauto FunctionPointer = &Calculate;
    return Counter.Compute(FunctionPointer(FValue));
}

#if 0
int Disabled = TRACE_COUNT;
#endif
'''
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--managed-only',action='store_true'); options=parser.parse_args()
    base=ROOT/'data/validation'; base.mkdir(parents=True,exist_ok=True)
    reports=ROOT/'data/reports/semantic-highlights'; reports.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='highlight-',dir=base) as folder:
        root=Path(folder)
        (root/'types.hpp').write_text('#pragma once\nstruct Widget { int Field; int Compute(int Parameter) { return Parameter + Field; } };\ninline int Calculate(int Input) { return Input * 2; }\n','utf-8')
        (root/'main.cpp').write_text(SOURCE,'utf-8')
        (root/'compile_commands.json').write_text(json.dumps([{'directory':str(root),'file':str(root/'main.cpp'),'arguments':['clang++','-std=c++20','-I'+str(root),'-c',str(root/'main.cpp')]}]),'utf-8')
        for theme,language in ([] if options.managed_only else [('graphite','en'),('light-grey','fr')]):
            report=reports/f'{theme}.json'
            executable=Path(os.environ.get('CYCODE_TEST_GUI',ROOT/'CyCode.exe'))
            result=subprocess.run([str(executable),'--workspace',str(root),'--open','main.cpp','--theme',theme,'--language',language,
                '--test-frames','180','--test-wait-idle','--test-hidden','--test-highlights','--test-report',str(report),'--test-capture',str(reports/f'{theme}.bmp')],capture_output=True,timeout=50)
            if result.returncode:
                raise RuntimeError(result.stderr.decode(errors='replace')+' '+(ROOT/'data/reports/startup-error.txt').read_text('utf-8'))
            state=json.loads(report.read_text('utf-8'))
            assert state['hiddenWindow'] and state['highlight']['count']>0 and state['highlight']['revision']>0,state
            assert not state['error'] and not state['highlight']['error'] and not state['dirty'],state
            assert state['text']==SOURCE and (root/'main.cpp').read_text('utf-8')==SOURCE,'Coloring changed source text'
            print(f'{theme}/{language}: real Engine/clangd semantic colors in hidden desktop, {state["highlight"]["count"]} spans',flush=True)
        sys.path.insert(0,str(ROOT/'app/tests'))
        from csharp_fixture import create_csharp_fixture, CONSUMER
        solution=create_csharp_fixture(root/'managed')
        report=reports/'csharp.json'; executable=Path(os.environ.get('CYCODE_TEST_GUI',ROOT/'CyCode.exe'))
        result=subprocess.run([str(executable),'--workspace',str(solution),'--open','App/Consumer.cs','--theme','graphite',
            '--test-frames','180','--test-wait-idle','--test-hidden','--test-highlights','--test-report',str(report),'--test-capture',str(reports/'csharp.bmp')],capture_output=True,timeout=85)
        if result.returncode: raise RuntimeError(result.stderr.decode(errors='replace')+' '+(ROOT/'data/reports/startup-error.txt').read_text('utf-8'))
        state=json.loads(report.read_text('utf-8'))
        assert state['hiddenWindow'] and state['highlight']['count']>0 and not state['highlight']['error'] and not state['error'],state
        assert not state['dirty'] and state['text']==CONSUMER and (root/'managed/App/Consumer.cs').read_text('utf-8')==CONSUMER,state
        print(f'C#/Roslyn: real cross-project semantic colors in hidden desktop, {state["highlight"]["count"]} spans',flush=True)
    print('Semantic desktop colors passed without visible IDE windows.')
if __name__=='__main__': main()

"""Exercise real native symbol/refactoring/Undo workflows through actual ImGui input."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',action='append',help='Run only this named scenario (repeatable).')
    parser.add_argument('--ci',action='store_true')
    parser.add_argument('--hidden',action='store_true')
    parser.add_argument('--executable',type=Path,default=ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode'))
    parser.add_argument('--output',type=Path)
    options=parser.parse_args()
    if options.ci and os.environ.get('GITHUB_ACTIONS')!='true':
        parser.error('--ci requires an explicitly configured GitHub Actions runner')
    EXE=options.executable.resolve(strict=True)
    if options.ci and not EXE.is_relative_to(ROOT/'runtime'):
        parser.error('Use this checkout production executable.')
    parent=ROOT/('runtime/test-workspaces/semantic-ci' if options.ci else 'data/validation/semantic-ui')
    parent.mkdir(parents=True,exist_ok=True)
    workspace=Path(tempfile.mkdtemp(prefix='owned-',dir=parent)).resolve()
    reports=(options.output or ROOT/('runtime/reports/semantic-ci' if options.ci else 'data/reports')).resolve()
    reports.mkdir(parents=True,exist_ok=True)
    selected=options.case or []
    executed = []
    header = '#pragma once\nstruct Widget { int Value; };\nint Read(Widget);\n'
    implementation = '#include "widget.hpp"\nint Read(Widget w) { return w.Value; }\n'
    source = '#include "widget.hpp"\nint  Main(Widget w){const char* label="🛠";return w.Value+Read(w);}\n'
    (workspace/'widget.hpp').write_bytes(header.encode('utf-8'))
    (workspace/'widget.cpp').write_bytes(implementation.encode('utf-8'))
    (workspace/'main.cpp').write_bytes(source.encode('utf-8'))
    (workspace/'.clang-format').write_text('BasedOnStyle: LLVM\nIndentWidth: 4\nAllowShortFunctionsOnASingleLine: None\n','utf-8')
    (workspace/'compile_commands.json').write_text(json.dumps([{'directory': str(workspace), 'file': str(workspace/name),
        'arguments': ['clang++', '-std=c++20', '-I'+str(workspace), '-c', str(workspace/name)]} for name in ('main.cpp','widget.cpp')]),'utf-8')

    def chord(frame, *keys):
        keys=tuple('Primary' if key=='Ctrl' and sys.platform=='darwin' else key for key in keys)
        return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]
    def typed(frame,text): return {'frame':frame,'keys':[],'text':text}
    def action(frame,text):
        command={'Apply refactoring':'refactor.apply','Undo refactoring':'refactor.undo'}[text]
        return [{'frame':frame-1,'keys':[],'waitForCommand':command}]+chord(frame,'Ctrl','Shift','A')+[typed(frame+12,text)]+chord(frame+28,'Enter')
    at=source.index('Value')
    line=source[:at].count('\n')+1
    start=source.rfind('\n',0,at)+1
    column=len(source[start:at].encode('utf-16-le'))//2+1
    def run(name,events,frames=230,panel=None,theme='graphite',language='en'):
        report=reports/f'semantic-{name}.json'
        events=[{'frame':29,'keys':[],'waitForCommand':'refactor.rename'}]+events
        args=[str(EXE),'--workspace',str(workspace),'--open','main.cpp','--line',str(line),'--column',str(column),
              '--keymap','rider','--theme',theme,'--language',language,'--test-frames',str(frames),'--test-wait-idle','--test-report',str(report),
              '--test-keys',json.dumps(events),'--test-capture',str(reports/f'semantic-{name}.bmp')]
        if options.hidden or options.ci: args+=['--test-hidden']
        if panel: args+=['--capture-panel',panel]
        subprocess.run(args,check=True,timeout=80,capture_output=True)
        state=json.loads(report.read_text('utf-8'))
        assert state['error']=='',state
        for filename,text in (('main.cpp',source),('widget.hpp',header),('widget.cpp',implementation)):
            assert (workspace/filename).read_bytes()==text.encode('utf-8'), 'GUI test saved source unexpectedly'
        return state

    def check(name, events, validate, frames=230, **options):
        if selected and name not in selected: return
        state = run(name, events, frames, **options)
        assert validate(state),state
        executed.append(name)
        print('semantic '+name+': passed',flush=True)

    check('types',chord(30,'Ctrl','N')+[typed(45,'Widget')],
          lambda s: s['ui']['quickOpen'] and any(x['name']=='Widget' for x in s['symbols']['symbols']),170)
    check('type-open',chord(30,'Ctrl','N')+[typed(45,'Widget')]+chord(110,'Enter'),lambda s: s['active']=='widget.hpp',190)
    check('members',chord(30,'Ctrl','F12')+[typed(45,'Main')],lambda s: any(x['name']=='Main' for x in s['symbols']['symbols']),170)
    rename=chord(30,'Shift','F6')+chord(43,'Ctrl','A')+[typed(51,'Amount')]+chord(65,'Enter')
    check('rename-preview',rename,lambda s: s['refactoring']['state']=='preview' and len(s['refactoring']['files'])==3 and s['text']==source,210)
    for theme in ('graphite','light-grey','navy','mid-grey','amber-night','violet'):
        check('preview-'+theme,rename,lambda s: s['refactoring']['state']=='preview' and s['text']==source,
              210,panel='refactoring',theme=theme)
    check('preview-light-fr',rename,lambda s: s['refactoring']['state']=='preview' and s['text']==source,
          210,panel='refactoring',theme='light-grey',language='fr')
    check('rename-apply',rename+action(140,'Apply refactoring'),lambda s: s['refactoring']['state']=='applied' and s['active']=='main.cpp' and 'w.Amount' in s['text'],250)
    check('rename-undo',rename+action(140,'Apply refactoring')+action(250,'Undo refactoring'),lambda s: s['refactoring']['state']=='undone' and s['active']=='main.cpp' and s['text']==source,360)
    check('format',chord(30,'Ctrl','Alt','L'),lambda s: '\n    return w.Value + Read(w);' in s['text'],160)
    check('format-undo',chord(30,'Ctrl','Alt','L')+chord(120,'Ctrl','Z'),lambda s: s['text']==source,190)
    if set(selected)-set(executed): raise RuntimeError('Unknown scenarios: '+', '.join(set(selected)-set(executed)))
    print(f'Actual GUI: {len(executed)} semantic scenarios passed; source fixtures unchanged on disk.')

    if workspace.is_symlink() or workspace.resolve().parent!=parent.resolve():
        raise RuntimeError('Preserve a moved or redirected semantic fixture.')
    shutil.rmtree(workspace)

if __name__=='__main__':main()

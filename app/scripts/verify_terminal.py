"""Exercise actual terminal keyboard input and editor isolation in the native GUI."""
import json
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
workspace = ROOT/'data/validation/terminal-ui'
workspace.mkdir(parents=True, exist_ok=True)
original = 'int terminal_must_not_edit_this = 42;\n'
(workspace/'main.cpp').write_text(original,'utf-8')
reports = ROOT/'data/reports'
reports.mkdir(parents=True, exist_ok=True)
def chord(frame,*keys): return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]
exe = ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode')
for theme,language in [('graphite','en'),('light-grey','fr')]:
    report = reports/f'terminal-{theme}.json'
    command = "set /a CyCodeAnswer=6*7" if sys.platform=='win32' else "printf 'GUI_%s\\n' $((6*7))"
    events = chord(20,'Alt','F12')+[{'frame':100,'keys':[],'text':command}]+chord(105,'Enter')
    if sys.platform=='win32': events += [{'frame':130,'keys':[],'text':'echo GUI_%CyCodeAnswer%'}]+chord(135,'Enter')
    # Ctrl+F belongs to the shell, not the IDE document-search command.
    events += chord(160,'Ctrl','F')+chord(170,'Escape')
    subprocess.run([str(exe),'--workspace',str(workspace),'--open','main.cpp','--theme',theme,'--language',language,
        '--keymap','rider','--terminal-profile','cmd' if sys.platform=='win32' else 'default','--test-frames','225','--test-wait-idle','--test-keys',json.dumps(events),
        '--test-report',str(report),'--test-capture',str(reports/f'terminal-{theme}.bmp'),'--capture-panel','terminal'],
        check=True,timeout=40,capture_output=True)
    state=json.loads(report.read_text('utf-8'))
    assert state['error']=='',state
    assert any('GUI_42' in line for line in state['terminal']['lines']),state['terminal']
    assert state['text']==original and (workspace/'main.cpp').read_text('utf-8')==original,state
    assert 'search.document' not in state['ui']['commands'],state['ui']
    assert len(state['terminals']['sessions'])==1,state['terminals']
    print(f'Actual GUI terminal input, shell output, document isolation and {theme}/{language} render: passed',flush=True)

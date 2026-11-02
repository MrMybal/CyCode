"""Drive actual debug/run shortcuts, source navigation and terminal output in CyCode."""
import json
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
workspace=ROOT/'data/validation/debug-native'
program=ROOT/('runtime/build-vs/Release/cycode_debug_fixture.exe' if sys.platform=='win32' else 'runtime/build/cycode_debug_fixture')
source=(workspace/'debuggee.cpp').read_text('utf-8')
breakpoint=next(i for i,line in enumerate(source.splitlines(),1) if 'BREAK_HERE' in line)
reports=ROOT/'data/reports'
def chord(frame,*keys): return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]
def action(frame,text): return chord(frame,'Ctrl','Shift','A')+[{'frame':frame+12,'keys':[],'text':text}]+chord(frame+28,'Enter')
def run(name,events,frames=230,theme='graphite',language='en',panel='debugger'):
    profile={'name':'Validation','program':str(program),'cwd':'.','args':[],'env':{},'adapter':'automatic',
             'stopOnEntry':False,'terminal':'integrated','buildBeforeLaunch':False,'watches':['value + 1']}
    (workspace/'.cycode').mkdir(exist_ok=True)
    (workspace/'.cycode/launch.json').write_text(json.dumps({'profile':profile,'breakpoints':{'debuggee.cpp':[{'line':breakpoint}]}}),'utf-8')
    report=reports/f'debugger-{name}.json'
    exe=ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode')
    subprocess.run([str(exe),'--workspace',str(workspace),'--open','debuggee.cpp','--keymap','rider','--theme',theme,'--language',language,
        '--test-frames',str(frames),'--test-wait-idle','--test-keys',json.dumps(events),'--test-report',str(report),
        '--test-capture',str(reports/f'debugger-{name}.bmp'),'--capture-panel',panel],check=True,timeout=55,capture_output=True)
    state=json.loads(report.read_text('utf-8'))
    assert state['error']=='',state
    assert state['debug']['error']=='',state['debug']
    assert (workspace/'debuggee.cpp').read_text('utf-8')==source,'Debugger modified source on disk'
    return state
start=chord(35,'Shift','F9')+[{'frame':100,'keys':[],'waitForCommand':'debug.next'}]
for theme,language in [('graphite','en'),('light-grey','fr')]:
    state=run('paused-'+theme,start,theme=theme,language=language)
    assert state['debug']['state']=='stopped',state['debug']
    assert state['active']=='debuggee.cpp' and state['cursor']['line']==breakpoint-1,state
    assert any(v['name']=='value' and v['value']=='41' for v in state['debug']['variables']),state['debug']
    assert any('42' in v.get('result','') for v in state['debug']['watches']),state['debug']['watches']
    print(f'Actual GUI breakpoint, source jump, locals, watch and {theme}/{language}: passed',flush=True)
state=run('step-over',start+chord(160,'F8'),260)
assert state['debug']['state']=='stopped' and any(v['name']=='value' and v['value']=='42' for v in state['debug']['variables']),state['debug']
print('Actual GUI F8 step over and updated local: passed',flush=True)
state=run('continue',start+chord(160,'F9')+action(200,'Program terminal'),300,panel='terminal')
assert state['debug']['state']=='exited',state['debug']
assert any('RESULT=42' in line for line in state['terminal']['lines']),state['terminal']
print('Actual GUI continue, process exit and integrated program I/O: passed',flush=True)
state=run('stop',start+chord(160,'Ctrl','F2'),240)
assert state['debug']['state']=='idle' and not state['terminals']['sessions'],state
print('Actual GUI stop terminates the launched target and its terminal: passed',flush=True)
state=run('run',chord(35,'Shift','F10'),220,panel='terminal')
assert any('RESULT=42' in line for line in state['terminal']['lines']),state['terminal']
assert state['debug']['state']=='idle',state['debug']
print('Actual GUI run without debugging and terminal output: passed',flush=True)

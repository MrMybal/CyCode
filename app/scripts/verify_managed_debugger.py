"""Exercise actual .NET DAP UI commands, source navigation, variables and program I/O."""
import json
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'app/tests'))
from managed_debug_fixture import create_managed_debug_fixture

def main():
    workspace = ROOT/'data/validation/debug-managed-ui'
    program,source,breakpoint = create_managed_debug_fixture(workspace)
    reports = ROOT/'data/reports'
    reports.mkdir(parents=True,exist_ok=True)
    def chord(frame,*keys):
        return [{'frame':frame,'keys':keys},{'frame':frame+2,'keys':keys,'down':False}]
    def run(name,events,frames=260,theme='graphite',locale='en',panel='debugger',args=None):
        profile = {'name':'Managed validation','program':str(program),'runtime':'automatic','dotnet':'automatic','adapter':'automatic',
            'cwd':'.','args':args or [],'env':{},'stopOnEntry':False,'terminal':'integrated','buildBeforeLaunch':False,'watches':['value + 1']}
        (workspace/'.cycode').mkdir(exist_ok=True)
        (workspace/'.cycode/launch.json').write_text(json.dumps({'profile':profile,'breakpoints':{'Program.cs':[{'line':breakpoint,'condition':'value == 41'}]}}),'utf-8')
        report = reports/f'managed-debug-{name}.json'
        subprocess.run([str(ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode')),'--workspace',str(workspace/'Managed.csproj'),'--open','Program.cs',
            '--theme',theme,'--language',locale,'--keymap','rider','--test-frames',str(frames),'--test-wait-idle','--test-report',str(report),
            '--test-keys',json.dumps(events),'--test-capture',str(reports/f'managed-debug-{name}.bmp'),'--capture-panel',panel],check=True,timeout=65,capture_output=True)
        state = json.loads(report.read_text('utf-8'))
        if state['error'] or state['debug']['error'] or (workspace/'Program.cs').read_text('utf-8') != source:
            raise RuntimeError(state)
        print('managed-debug-'+name+': rendered',flush=True)
        return state
    start = chord(35,'Shift','F9')+[{'frame':100,'keys':[],'waitForCommand':'debug.next'}]
    for theme,locale in [('graphite','en'),('light-grey','fr')]:
        state = run('paused-'+theme,start,theme=theme,locale=locale)
        if state['debug']['state'] != 'stopped' or state['debug']['runtime'] != 'dotnet' or state['active'] != 'Program.cs' or state['cursor']['line'] != breakpoint-1:
            raise RuntimeError(state)
        if not any(v['name']=='value' and v['value']=='41' for v in state['debug']['variables']) or not any(v.get('result')=='42' for v in state['debug']['watches']):
            raise RuntimeError(state['debug'])
        if not any(v.get('verified') for v in state['debug']['verifiedBreakpoints']['Program.cs']):
            raise RuntimeError('Deferred .NET breakpoint never became verified')
    state = run('step-over',start+chord(170,'F8'),frames=280)
    if state['debug']['state'] != 'stopped' or not any(v['name']=='value' and v['value']=='42' for v in state['debug']['variables']):
        raise RuntimeError(state['debug'])
    state = run('continue',start+chord(170,'F9'),frames=280)
    if state['debug']['state'] != 'exited' or 'RESULT=42' not in state['debug']['output']:
        raise RuntimeError(state['debug'])
    state = run('stop',start+chord(170,'Ctrl','F2'),frames=250)
    if state['debug']['state'] != 'idle':
        raise RuntimeError(state['debug'])
    state = run('run',chord(35,'Shift','F10'),panel='terminal',args=['plain'])
    if not any('RESULT=42 ARG=plain' in line for line in state['terminal']['lines']) or state['debug']['state'] != 'idle':
        raise RuntimeError(state)
    print('Managed desktop acceptance passed: portable PDB source stops, locals/watches, verified breakpoints, Rider step/continue/stop/run and Graphite/light French renders.')

if __name__ == '__main__':
    main()

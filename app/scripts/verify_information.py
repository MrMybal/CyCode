"""Actual documentation/parameter shortcuts. All application windows stay hidden."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]

def chord(frame,*keys):
    return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]

def main():
    executable=ROOT/'CyCode.exe'
    if not executable.is_file():
        raise SystemExit('CyCode.exe is absent. Resolve the application/protection report before running GUI acceptance; this script does not restore executables or change protection settings.')
    reports=ROOT/'data/reports/information';reports.mkdir(parents=True,exist_ok=True)
    base=ROOT/'data/validation';base.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='information-',dir=base) as directory:
        root=Path(directory)
        text='/// Combine two values.\nint combine(int left, int right) { return left + right; }\nint main() { return combine(1, 2); }\n'
        (root/'main.cpp').write_text(text,'utf-8')
        for profile in ('rider','visualstudio','vscode'):
            for mode in ('hover','signature'):
                offset=text.rfind('combine(')+2 if mode=='hover' else text.rfind('2);')
                prefix=text[:offset];line=prefix.count('\n')+1;column=len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2+1
                command='code.documentation' if mode=='hover' else 'code.parameters'
                events=[{'frame':35,'keys':[],'waitForCommand':command}]
                if mode=='hover':
                    events+=chord(36,'Ctrl','Q') if profile=='rider' else chord(36,'Ctrl','K')+chord(43,'Ctrl','I')
                else: events+=chord(36,'Ctrl','P') if profile=='rider' else chord(36,'Ctrl','Shift','Space')
                name=profile+'-'+mode;report=reports/(name+'.json');report.unlink(missing_ok=True)
                argv=[str(executable),'--workspace',str(root),'--open','main.cpp','--line',str(line),'--column',str(column),
                    '--keymap',profile,'--theme','light-grey' if profile=='visualstudio' else 'graphite',
                    '--test-hidden','--test-frames','190','--test-wait-idle','--test-keys',json.dumps(events),
                    '--test-report',str(report),'--test-capture',str(reports/(name+'.bmp')),'--capture-panel','information']
                result=subprocess.run(argv,capture_output=True,timeout=70)
                assert result.returncode==0 and report.is_file(),(name,result.returncode,result.stderr.decode(errors='replace'))
                state=json.loads(report.read_text('utf-8'));information=state['information']['result']
                assert state['hiddenWindow'] and not state['error'] and not state['information']['error'],state
                assert command in state['ui']['commands'] and information['available'],state
                assert state['information']['method']=='language.'+mode,state
                if mode=='signature':
                    selected=information['signatures'][information['activeSignature']]
                    assert selected['activeParameter']==1 and selected['parameters'][1]['label']=='int right',selected
                assert not state['dirty'] and (root/'main.cpp').read_text('utf-8')==text,'Information query edited the source'
                print(name+': actual shortcut, live server information and hidden window passed',flush=True)

if __name__=='__main__': main()

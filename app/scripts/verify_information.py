"""Actual documentation/parameter shortcuts. All application windows stay hidden."""
import argparse
import hashlib
import os
import sys
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]

def chord(frame,*keys):
    keys=tuple('Primary' if key=='Ctrl' and sys.platform=='darwin' else key for key in keys)
    return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,default=ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode'))
    parser.add_argument('--output',type=Path)
    parser.add_argument('--ci',action='store_true')
    args=parser.parse_args()
    if args.ci and os.environ.get('GITHUB_ACTIONS')!='true':
        parser.error('--ci requires an explicit GitHub Actions runner')
    executable=args.executable.resolve(strict=True)
    if args.ci and not executable.is_relative_to(ROOT/'runtime'):
        parser.error('Use this checkout production executable.')
    reports=(args.output or ROOT/'data/reports/information').resolve();reports.mkdir(parents=True,exist_ok=True)
    base=ROOT/('runtime/test-workspaces/information-ci' if args.ci else 'data/validation');base.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='information-',dir=base) as directory:
        root=Path(directory)
        text='/// Combine two values.\nint combine(int left, int right) { return left + right; }\nint main() { return combine(1, 2); }\n'
        (root/'main.cpp').write_bytes(text.encode('utf-8'))
        source_digest=hashlib.sha256((root/'main.cpp').read_bytes()).hexdigest()
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
                assert abs(state['scriptedInput']['fixedDeltaSeconds']-1/60)<1e-9,state['scriptedInput']
                assert state['hiddenWindow'] and not state['error'] and not state['information']['error'],state
                assert command in state['ui']['commands'] and information['available'],state
                assert state['information']['method']=='language.'+mode,state
                if mode=='signature':
                    selected=information['signatures'][information['activeSignature']]
                    assert selected['activeParameter']==1 and selected['parameters'][1]['label']=='int right',selected
                assert not state['dirty'] and hashlib.sha256((root/'main.cpp').read_bytes()).hexdigest()==source_digest,'Information query edited the source'
                print(name+': actual shortcut, live server information and hidden window passed',flush=True)

if __name__=='__main__': main()

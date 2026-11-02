"""Actual GUI navigation from an owned Unreal project into a read-only engine header."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
report_dir = ROOT / 'data/reports'
previous = json.loads((report_dir / 'unreal-completion-actor.json').read_text('utf-8'))
workspace = Path(previous['workspace']).resolve()
if not workspace.is_relative_to((ROOT/'data/validation').resolve()):
    raise RuntimeError('Unreal acceptance requires an owned validation workspace')
project = next(workspace.glob('*.uproject'))
module = next((workspace/'Source').glob('*/CyCodeCompletion.Build.cs')).parent
source = module/'SdkNavigation.cpp'
text = '#include "GameFramework/Actor.h"\nvoid InspectActor(AActor* Actor) { (void)Actor; }\n'
source.write_text(text,'utf-8')
at=text.index('AActor'); offset=text.rfind('\n',0,at)+1
position={'line':text[:at].count('\n'),'character':len(text[offset:at].encode('utf-16-le'))//2}
relative=source.relative_to(workspace).as_posix()
report=report_dir/'unreal-sdk-navigation.json'
exe=ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode')
subprocess.run([str(exe),'--workspace',str(project),'--open',relative,'--test-frames','200','--test-wait-idle',
    '--test-navigation',json.dumps({'path':relative,'position':position}), '--test-report',str(report),
    '--test-capture',str(report_dir/'unreal-sdk-navigation.bmp')],check=True,timeout=200,capture_output=True)
state=json.loads(report.read_text('utf-8'))
assert state['error']=='',state
assert state['active'].startswith('sdk://') and state['readOnly'],state
assert state['displayPath'].replace('\\','/').endswith('/Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h'),state['displayPath']
assert 'class ENGINE_API AActor' in state['text'] or 'class AActor' in state['text'],state['displayPath']
assert source.read_text('utf-8')==text,'Navigation test saved disk source'
print('Actual Unreal GUI: AActor definition opens the engine Actor.h as a read-only SDK tab; project source unchanged.')

"""Real C# navigation, completion and formatting through native desktop commands."""
import json
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'app/tests'))
from csharp_fixture import create_csharp_fixture, CONSUMER, LIBRARY

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preview-only',action='store_true')
    options = parser.parse_args()
    fixtures = ROOT/'data/validation'
    fixtures.mkdir(parents=True,exist_ok=True)
    reports = ROOT/'data/reports'
    reports.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='csharp-',dir=fixtures) as temporary:
        root = Path(temporary)
        solution = create_csharp_fixture(root)
        executable = ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode')
        def run(label, extra, theme='graphite', locale='en', keys=None):
            report, capture = reports/f'csharp-{label}.json', reports/f'csharp-{label}.bmp'
            report.unlink(missing_ok=True)
            argv = [str(executable),'--workspace',str(solution),'--open','App/Consumer.cs','--theme',theme,'--language',locale,
                    '--test-frames','200','--test-report',str(report),'--test-capture',str(capture),*extra]
            if keys:
                argv += ['--test-keys',json.dumps(keys)]
            subprocess.run(argv,cwd=root.parent,check=True,timeout=100)
            state = json.loads(report.read_text('utf-8'))
            if state['error'] or not state['languageReady'] or not capture.exists():
                raise RuntimeError(state)
            print(f'csharp-{label}: rendered',flush=True)
            return state
        position = {'line':1,'character':CONSUMER.splitlines()[1].index('Value')}
        if options.preview_only:
            def chord(frame,*keys):
                return [{'frame':frame,'keys':keys},{'frame':frame+2,'keys':keys,'down':False}]
            keys = chord(60,'Shift','F6')+chord(76,'Ctrl','A')+[{'frame':87,'keys':[],'text':'Amount'}]+chord(101,'Enter')
            for theme,locale in (('graphite','en'),('light-grey','fr')):
                state = run('rename-preview-'+theme,['--line','2','--column',str(position['character']+1),
                    '--test-wait-idle','--capture-panel','refactoring'],theme=theme,locale=locale,keys=keys)
                if state['refactoring']['state'] != 'preview' or len(state['refactoring']['files']) != 2 or state['text'] != CONSUMER:
                    raise RuntimeError(state)
            print('C# rename previews: dark/light French rendered; source buffers and files unchanged.')
            return
        definition = run('definition',['--test-navigation',json.dumps({'path':'App/Consumer.cs','position':position})])
        if definition['active'] != 'Library/Counter.cs' or definition['cursor']['line'] != 1:
            raise RuntimeError(definition)
        references = run('references-fr',['--test-navigation',json.dumps({'path':'App/Consumer.cs','position':position,'references':True})],locale='fr')
        if len(references['navigation']['locations']) != 2:
            raise RuntimeError(references)
        incomplete = CONSUMER.replace('c.Value','c.Vis')
        (root/'App/Consumer.cs').write_text(incomplete,'utf-8')
        completion_pos = {'line':1,'character':incomplete.splitlines()[1].index('c.Vis')+5}
        action = {'path':'App/Consumer.cs','position':completion_pos}
        completion = run('completion',['--test-completion',json.dumps(action)])
        if 'VisibleMember' not in completion.get('completionSuggestions',[]):
            raise RuntimeError(completion)
        accepted = run('completion-accept-light',['--test-completion',json.dumps({**action,'acceptFirst':True})],theme='light-grey',locale='fr')
        if 'c.VisibleMember' not in accepted.get('text','') or not accepted.get('dirty'):
            raise RuntimeError(accepted)
        if (root/'App/Consumer.cs').read_text('utf-8') != incomplete or (root/'Library/Counter.cs').read_text('utf-8') != LIBRARY:
            raise RuntimeError('GUI completion unexpectedly saved project sources')
        (root/'App/Consumer.cs').write_text('public  class Consumer{public   int Value=3;}\n','utf-8')
        # Real Rider Format document shortcut.
        keys = [{'frame':100,'keys':['Ctrl','Alt','L']},{'frame':101,'keys':['L','Alt','Ctrl'],'down':False}]
        formatted = run('format',['--test-wait-idle'],keys=keys)
        if not formatted.get('dirty') or 'public  class' in formatted.get('text',''):
            raise RuntimeError(formatted)
        print('C# desktop acceptance passed: project references, navigation/references, completion/insertion and Roslyn formatting; sources remained unsaved.')

if __name__ == '__main__':
    main()

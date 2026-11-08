"""Render real clangd navigation through the desktop's command handlers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ci',action='store_true')
    parser.add_argument('--executable',type=Path,default=ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode'))
    parser.add_argument('--output',type=Path)
    options=parser.parse_args()
    if options.ci and os.environ.get('GITHUB_ACTIONS')!='true':
        parser.error('--ci requires an explicitly configured GitHub Actions runner')
    executable=options.executable.resolve(strict=True)
    if options.ci and not executable.is_relative_to(ROOT/'runtime'):
        parser.error('Use this checkout production executable.')
    REPORT=(options.output or ROOT/('runtime/reports/navigation-ci' if options.ci else 'data/reports')).resolve()
    REPORT.mkdir(parents=True,exist_ok=True)
    fixtures = ROOT / ('runtime/test-workspaces/navigation-ci' if options.ci else 'data/validation')
    fixtures.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='navigation-', dir=fixtures) as temporary:
        workspace = Path(temporary)
        name = 'résumé #1.cpp'
        text = 'const char* label = "🛠"; int answer = 7;\nint main() { return answer + answer; }\n'
        (workspace / name).write_bytes(text.encode('utf-8'))
        source_digest=hashlib.sha256((workspace/name).read_bytes()).hexdigest()
        (workspace / 'compile_flags.txt').write_text('-std=c++20\n', encoding='utf-8')
        position = {'line': 1, 'character': text.splitlines()[1].index('answer')}
        expected_character = len(text.split('answer')[0].encode('utf-16-le')) // 2
        expected_codepoint = text.index('answer')
        for references, theme, language in ((False, 'navy', 'en'), (True, 'navy', 'fr'), (True, 'light-grey', 'en')):
            label = f'navigation-{"references" if references else "definition"}-{theme}-{language}'
            report, capture = REPORT / f'{label}.json', REPORT / f'{label}.bmp'
            report.unlink(missing_ok=True)
            capture.unlink(missing_ok=True)
            action = {'path': name, 'position': position, 'references': references}
            startup = None
            if sys.platform == 'win32':
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            subprocess.run([str(executable), '--workspace', str(workspace), '--open', name,
                '--theme', theme, '--language', language, '--test-frames', '150', '--test-report', str(report),
                '--test-hidden','--test-wait-idle','--test-capture', str(capture), '--test-navigation', json.dumps(action, ensure_ascii=False)],
                cwd=workspace.parent, startupinfo=startup, check=True, timeout=65)
            state = json.loads(report.read_text('utf-8'))
            if state['error'] or not state['languageReady']:
                raise RuntimeError(state)
            locations = state['navigation']['locations']
            if references:
                if len(locations) != 3:
                    raise RuntimeError(state)
            elif state['active'] != name or state['cursor'] != {'line': 0, 'codepoint': expected_codepoint, 'character': expected_character}:
                raise RuntimeError(f'Unicode definition jump failed: {state}')
            if not capture.exists() or hashlib.sha256((workspace/name).read_bytes()).hexdigest()!=source_digest:
                raise RuntimeError('Missing render or fixture changed')
            print(f'{label}: passed')

if __name__ == '__main__':
    main()

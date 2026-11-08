"""Exercise real ImGui key input against native documents, not direct command dispatch."""
import json
import argparse
import shutil
import tempfile
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=ROOT / ('CyCode.exe' if sys.platform == 'win32' else 'CyCode'))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--hidden', action='store_true')
    parser.add_argument('--ci', action='store_true')
    args = parser.parse_args()
    if args.ci and os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('--ci requires an explicitly configured GitHub Actions runner')
    executable = args.executable.resolve(strict=True)
    parent = ROOT / 'runtime/test-workspaces/keyboard'
    parent.mkdir(parents=True, exist_ok=True)
    workspace = Path(tempfile.mkdtemp(prefix='keys-', dir=parent))
    reports = (args.output or ROOT / 'data/reports').resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    reports.mkdir(parents=True, exist_ok=True)
    original = 'int First = 1;\nint Second = 2;\n'
    (workspace / 'First.cpp').write_bytes(original.encode('utf-8'))
    (workspace / 'Second.cpp').write_bytes(b'int GlobalNeedle = 42;\n')
    environment = {key.upper(): value for key, value in os.environ.items()}
    if sys.platform == 'win32':
        environment['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')

    def chord(frame, *keys):
        keys=tuple('Primary' if key=='Ctrl' and sys.platform=='darwin' else key for key in keys)
        return [{'frame': frame, 'keys': keys, 'down': True}, {'frame': frame+2, 'keys': keys, 'down': False}]

    def typed(frame, text):
        return {'frame': frame, 'keys': [], 'text': text}

    def run(name, profile, events, opened=True):
        report = reports / f'keys-{name}.json'
        report.unlink(missing_ok=True)
        args_list = [str(executable), '--workspace', str(workspace), '--keymap', profile, '--language', 'en',
                '--test-frames', '115', '--test-wait-idle', '--test-report', str(report), '--test-keys', json.dumps(events)]
        if args.hidden:
            args_list += ['--test-hidden']
        if opened:
            args_list += ['--open', 'First.cpp']
        if name == 'keymap-settings': args_list += ['--test-capture', str(reports / 'keymap-settings.bmp'), '--capture-panel', 'options']
        subprocess.run(args_list, env=environment, cwd=ROOT.parent, timeout=25, check=True, capture_output=True)
        state = json.loads(report.read_text('utf-8'))
        assert state['error'] == '', (name, state['error'])
        assert state['fileCount'] == 2, (name, state['fileCount'])
        assert state['textScale'] >= 1.1, state['textScale']
        assert (workspace / 'First.cpp').read_text('utf-8') == original, 'Test edited disk source'
        print(name + ': passed', flush=True)
        return state

    state = run('rider-editing', 'rider', chord(20, 'Ctrl', 'D') + chord(42, 'Ctrl', 'Y') + chord(65, 'Ctrl', 'Z'))
    assert state['text'].count('int First') == 2, state
    assert state['ui']['commands'] == ['edit.duplicate', 'edit.deleteLine', 'edit.undo'], state['ui']
    for profile in ('visualstudio', 'vscode'):
        state = run(profile+'-chords', profile, chord(20, 'Ctrl', 'K') + chord(29, 'Ctrl', 'C') + chord(50, 'Ctrl', 'K') + chord(59, 'Ctrl', 'U'))
        assert state['text'] == original, state['text']
        assert state['ui']['commands'] == ['edit.addComment', 'edit.removeComment'], state['ui']
    state = run('global-search', 'rider', chord(20, 'Ctrl', 'Shift', 'F') + [typed(32, 'GlobalNeedle')])
    assert state['ui']['commands'] == ['search.workspace'], state['ui']
    assert any(item['path'] == 'Second.cpp' for item in state['ui']['search']['matches']), state['ui']
    state = run('document-search', 'rider', chord(20, 'Ctrl', 'F') + [typed(32, 'Second')])
    assert state['text'] == original and state['ui']['commands'] == ['search.document'], state
    state = run('file-search', 'rider', chord(20, 'Ctrl', 'Alt', 'F') + [typed(32, 'Second.cpp')] + chord(55, 'Enter'), opened=False)
    assert state['active'] == 'Second.cpp', state
    state = run('double-shift', 'rider', chord(20, 'Shift') + chord(27, 'Shift'), opened=False)
    assert state['ui']['quickOpen'] and state['ui']['paletteMode'] == 'everywhere', state
    state = run('shift-not-a-chord', 'rider', chord(20, 'Shift', 'A') + chord(27, 'Shift'))
    assert not state['ui']['quickOpen'], state['ui']
    state = run('action-palette', 'vscode', chord(20, 'Ctrl', 'Shift', 'P') + [typed(32, 'Toggle word wrap')] + chord(55, 'Enter'))
    assert 'view.wrap' in state['ui']['commands'], state['ui']
    state = run('keymap-settings', 'vscode', chord(20, 'Ctrl', 'K') + chord(29, 'Ctrl', 'S'), opened=False)
    assert state['ui']['options'], state['ui']
    state = run('close-reopen', 'rider', chord(20, 'Ctrl', 'F4') + chord(42, 'Ctrl', 'Shift', 'T'))
    assert state['active'] == 'First.cpp' and state['ui']['commands'] == ['file.close', 'file.reopen'], state
    state = run('line-and-history', 'rider', chord(20, 'Ctrl', 'G') + chord(32, 'Ctrl', 'A') + [typed(40, '2')] + chord(50, 'Enter') + chord(70, 'Ctrl', 'Alt', 'Left'))
    assert state['cursor']['line'] == 0 and state['ui']['commands'] == ['navigate.line', 'navigate.back'], state
    print('Keyboard profiles, document edits, comment chords, search contexts, file opening, double Shift and command palette passed.')

    if workspace.is_symlink() or workspace.resolve().parent != parent.resolve():
        raise RuntimeError('Preserve a redirected or moved keyboard fixture')
    shutil.rmtree(workspace)  # Exact verified owned child; failed fixtures retain evidence.

if __name__ == '__main__':
    main()

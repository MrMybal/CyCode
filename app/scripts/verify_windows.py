"""Real native viewports and independent simultaneous GUI/Core sessions."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'app'))
from tool import NativeClient

def endpoint(workspace, process):
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline and process.poll() is None:
        for descriptor in (ROOT / 'data/bridge').glob('native-*.json'):
            try:
                data = json.loads(descriptor.read_text('utf-8'))
                if Path(data['workspace']).resolve() == workspace.resolve():
                    core = descriptor.with_name(descriptor.name.replace('native-', 'core-', 1))
                    service = json.loads(core.read_text('utf-8'))
                    return descriptor, core, service['url']
            except (OSError, ValueError, KeyError):
                continue
        time.sleep(.03)
    raise RuntimeError('GUI/Core session did not start')

def main():
    executable = ROOT / ('CyCode.exe' if sys.platform == 'win32' else 'CyCode')
    reports = ROOT / 'data/reports'
    reports.mkdir(parents=True, exist_ok=True)
    processes = []
    fixtures = ROOT / 'data/validation'
    fixtures.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='windows-', dir=fixtures) as temporary:
        try:
            workspaces = [Path(temporary) / name for name in ('first', 'second')]
            endpoints = []
            for i, workspace in enumerate(workspaces):
                workspace.mkdir()
                (workspace / 'main.cpp').write_text(f'int answer = {i + 1};\n', encoding='utf-8')
                report = reports / f'windows-instance-{i}.json'
                report.unlink(missing_ok=True)
                process = subprocess.Popen([str(executable), '--workspace', str(workspace), '--open', 'main.cpp',
                    '--theme', 'graphite', '--test-windows', '--test-frames', str(360 if i == 0 else 800),
                    '--test-report', str(report)], cwd=workspace.parent)
                processes.append(process)
                endpoints.append(endpoint(workspace, process))
            if endpoints[0][0] == endpoints[1][0] or endpoints[0][2] == endpoints[1][2]:
                raise RuntimeError('Two instances share a service endpoint')
            clients = [NativeClient(e[0]) for e in endpoints]
            before = clients[0].call('documents.read', {'path': 'main.cpp'})
            clients[0].call('documents.replace', {'path': 'main.cpp', 'text': 'int answer = 99;\n', 'expectedRevision': before['revision']})
            if clients[1].call('documents.read', {'path': 'main.cpp'})['text'] != 'int answer = 2;\n':
                raise RuntimeError('Live buffers leaked across instances')
            processes[0].wait(timeout=35)
            if processes[0].returncode or processes[1].poll() is not None:
                raise RuntimeError('Closing the first instance stopped the second')
            if clients[1].call('documents.read', {'path': 'main.cpp'})['text'] != 'int answer = 2;\n':
                raise RuntimeError('Second service stopped after first shutdown')
            processes[1].wait(timeout=35)
            for i, process in enumerate(processes):
                if process.returncode:
                    raise RuntimeError(f'GUI instance {i} failed')
                state = json.loads((reports / f'windows-instance-{i}.json').read_text('utf-8'))
                stages = {stage['stage']: stage for stage in state['windowTest']}
                if state['error'] or not stages['detached']['panelNative'] or not stages['detached']['editorNative'] or stages['detached']['nativeWindows'] != 3:
                    raise RuntimeError(f'Native detachment failed: {stages}')
                if not stages['closed']['panelHidden'] or not stages['closed']['editorDocked']:
                    raise RuntimeError(f'Native close/redocking failed: {stages}')
                if stages['restored']['nativeWindows'] != 1 or not stages['restored']['panelVisible'] or not stages['restored']['panelDocked']:
                    raise RuntimeError(f'Panel restore failed: {stages}')
                if (workspaces[i] / 'main.cpp').read_text('utf-8') != f'int answer = {i + 1};\n':
                    raise RuntimeError('Test unexpectedly saved a live buffer')
                if endpoints[i][0].exists() or endpoints[i][1].exists():
                    raise RuntimeError('Instance descriptor survived shutdown')
            print('Passed: native editor/panel detachment, move/resize, close/reopen/redock; two independent GUI/Core instances and unsaved buffers; independent shutdown.')
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)

if __name__ == '__main__':
    main()

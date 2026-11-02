"""Real CMake project configuration, target build and CyUI renders in an owned workspace."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
REPORTS = ROOT / 'data/reports'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', type=Path, required=True)
    args = parser.parse_args()
    cli = str(args.cli.resolve())
    REPORTS.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cycode-cmake-desktop-') as temporary:
        workspace = Path(temporary) / 'projet été'
        workspace.mkdir()
        (workspace / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(CyCodeCmakeDesktop LANGUAGES CXX)\nadd_executable(demo main.cpp)\ntarget_compile_definitions(demo PRIVATE CYCODE_CONFIGURED=1)\ntarget_compile_features(demo PRIVATE cxx_std_20)\n', encoding='utf-8')
        text = '// CMake supplies the compiler settings to clangd.\n#ifndef CYCODE_CONFIGURED\n#error Configure the project first\n#endif\nint main() { return 0; }\n'
        (workspace / 'main.cpp').write_text(text, encoding='utf-8')
        def operation(method, parameters):
            process = subprocess.run([cli, str(workspace), method, json.dumps(parameters, ensure_ascii=False)], capture_output=True, check=True, timeout=200)
            result = json.loads(process.stdout)
            if 'exitCode' in result and result['exitCode']:
                raise RuntimeError(result['output'])
            return result
        tools = operation('toolchains.detect', {})
        compiler = tools['toolchains'][0]
        operation('build.configure', {'buildDirectory': 'build with spaces', 'configuration': 'Release', 'toolchain': compiler['id']})
        built = operation('build.run', {'target': 'demo'})
        if not built['project']['compilationDatabase']['available']:
            raise RuntimeError('Compilation database was not prepared')
        executable = ROOT / ('CyCode.exe' if sys.platform == 'win32' else 'CyCode')
        for theme, language in (('navy', 'fr'), ('light-grey', 'en'), ('mid-grey', 'en'), ('amber-night', 'en'), ('violet', 'fr')):
            name = f'cmake-{theme}-{language}'
            report, capture = REPORTS / f'{name}.json', REPORTS / f'{name}.bmp'
            report.unlink(missing_ok=True); capture.unlink(missing_ok=True)
            startup = None
            if sys.platform == 'win32':
                startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
            subprocess.run([str(executable), '--workspace', str(workspace), '--open', 'main.cpp', '--show-cmake',
                '--theme', theme, '--language', language, '--test-frames', '120', '--test-report', str(report), '--test-capture', str(capture)],
                cwd=workspace.parent, startupinfo=startup, timeout=25, check=True)
            state = json.loads(report.read_text('utf-8'))
            project = state['build']
            if state['error'] or not project['compilationDatabase']['available'] or project['profile']['target'] != 'demo':
                raise RuntimeError(state)
            if not state['toolchains']['toolchains'] or not state['languageReady'] or not capture.exists():
                raise RuntimeError('Desktop toolchain detection, clangd or render failed')
            if (workspace / 'main.cpp').read_text('utf-8') != text:
                raise RuntimeError('Source fixture changed')
            print(f'{name}: profile, flags, detection and render passed')

if __name__ == '__main__':
    main()

"""Portable build entry point. Changes only this project's runtime directory."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--core-only', action='store_true')
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--cyui', type=Path, default=ROOT.parent / 'CyImgui_Interface')
    args = parser.parse_args()
    if not args.core_only:
        from fetch_runtime_tools import ensure_runtime_tools
        ensure_runtime_tools()
        from setup_debugger import ensure_debugger
        ensure_debugger()
        from setup_csharp import ensure_csharp
        ensure_csharp()
    environment = dict(os.environ)
    cmake = shutil.which('cmake')
    ninja = shutil.which('ninja')
    if sys.platform == 'win32':
        # Normalize duplicate Path/PATH keys inherited from some desktop hosts.
        environment = {key.upper(): value for key, value in os.environ.items()}
        environment['VSLANG'] = '1033'
        visual_studio = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Microsoft Visual Studio/2022/Community'
        vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        if vswhere.exists():
            detected = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath']).decode('utf-8').strip()
            if detected:
                visual_studio = Path(detected)
        if not cmake:
            cmake = str(visual_studio / 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
        if not ninja:
            ninja = str(visual_studio / 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe')
        vcvars = visual_studio / 'VC/Auxiliary/Build/vcvars64.bat'
        if vcvars.exists() and not shutil.which('cl'):
            output = subprocess.check_output(f'"{vcvars}" >nul && set', shell=True, env=environment).decode(errors='replace')
            environment.update(dict(line.split('=', 1) for line in output.splitlines() if '=' in line and not line.startswith('=')))
        environment = {key.upper(): value for key, value in environment.items()}
    if not cmake:
        raise SystemExit('Install CMake 3.24+ first.')
    build_name = ('core-build' if args.core_only else 'build') + ('-vs' if sys.platform == 'win32' else '')
    build = ROOT / 'runtime' / build_name
    build.mkdir(parents=True, exist_ok=True)
    log = build / 'build.log'
    generator = ['-G', 'Visual Studio 17 2022', '-A', 'x64'] if sys.platform == 'win32' else ['-G', 'Ninja', f'-DCMAKE_MAKE_PROGRAM={ninja or "ninja"}']
    commands = [[cmake, '-S', str(ROOT / 'app'), '-B', str(build), *generator,
                 f'-DCMAKE_BUILD_TYPE={args.configuration}', f'-DCYUI_DIR={args.cyui}',
                 f'-DCYCODE_BUILD_GUI={"OFF" if args.core_only else "ON"}', '-DBUILD_TESTING=ON'],
                [cmake, '--build', str(build), '--config', args.configuration, '--parallel', '4']]
    with log.open('w', encoding='utf-8') as stream:
        for command in commands:
            print(' '.join(command), flush=True)
            result = subprocess.run(command, env=environment, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
            stream.flush()
            if result.returncode:
                lines = log.read_text('utf-8', errors='replace').splitlines()
                errors = [line for line in lines if any(marker in line for marker in ('error C', 'error LNK', 'FAILED:', 'fatal error', 'CMake Error'))]
                print('\n'.join(errors[:30] or lines[-25:]))
                return result.returncode
    ctest = str(Path(cmake).with_name('ctest.exe' if sys.platform == 'win32' else 'ctest'))
    subprocess.run([ctest, '--test-dir', str(build), '-C', args.configuration, '--output-on-failure'], env=environment, check=True)
    if not args.core_only:
        executable = 'CyCode.exe' if sys.platform == 'win32' else 'CyCode'
        shutil.copy2(build / args.configuration / executable if sys.platform == 'win32' else build / executable, ROOT / executable)
    print(f'Build complete. Full log: {log}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())

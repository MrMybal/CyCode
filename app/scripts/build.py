"""Build the application. Native test binaries and execution are explicit opt-ins."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def build_commands(args, cmake, ninja, editor):
    # Separate caches avoid carrying BUILD_TESTING=ON into ordinary app builds.
    name = 'tests-core-build' if args.core_only else 'tests-build'
    if not args.build_tests:
        name = 'core-build' if args.core_only else 'build'
    build = ROOT / 'runtime' / (name + ('-vs' if sys.platform == 'win32' else ''))
    generator = ['-G', 'Visual Studio 17 2022', '-A', 'x64'] if sys.platform == 'win32' else [
        '-G', 'Ninja', f'-DCMAKE_MAKE_PROGRAM={ninja or "ninja"}']
    configure = [cmake, '-S', str(ROOT / 'app'), '-B', str(build), *generator,
                 f'-DCMAKE_BUILD_TYPE={args.configuration}', f'-DCYUI_DIR={args.cyui or ""}',
                 f'-DCYCODE_EDITOR_ROOT={editor}',
                 f'-DCYCODE_BUILD_GUI={"OFF" if args.core_only else "ON"}',
                 f'-DBUILD_TESTING={"ON" if args.build_tests else "OFF"}']
    compile_command = [cmake, '--build', str(build), '--config', args.configuration,
                       '--parallel', '4']
    if not args.build_tests:
        compile_command += ['--target', *([] if args.core_only else ['CyCode']),
                            'cycode_cli', 'cycode_sample', 'cycode_unreal', 'cycode_svn', 'cycode_perforce', 'cycode_mercurial', 'cycode_diversion']
    return build, [configure, compile_command]


def test_command(args, cmake, build):
    ctest = str(Path(cmake).with_name('ctest.exe' if sys.platform == 'win32' else 'ctest'))
    command = [ctest, '--test-dir', str(build), '-C', args.configuration,
               '--output-on-failure', '--stop-on-failure', '--no-tests=error', '--parallel', '1']
    if args.test_filter:
        command += ['--tests-regex', args.test_filter]
    return command


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--core-only', action='store_true')
    mode.add_argument('--reader-only', action='store_true', help='Deprecated: CyCodeReader now has its own repository/build')
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--cyui', type=Path, help='Clean checkout at app/cyui-source.lock.json (default: pinned cache)')
    parser.add_argument('--editor-root', type=Path, help='Development checkout of CyCodeReader (default: pinned dependency)')
    parser.add_argument('--no-install', action='store_true', help='Keep all build outputs in runtime; do not copy a root executable')
    parser.add_argument('--build-tests', action='store_true', help='Opt in to compiling native test executables in a separate build; does not run them or install the app')
    parser.add_argument('--run-tests', action='store_true', help='Opt in to building and running native tests (debuggers, child processes and generated programs included)')
    parser.add_argument('--test-filter', help='CTest name regex; requires --run-tests')
    parser.add_argument('--plan', action='store_true', help='Print the build/test commands without downloads, configuration, compilation or execution')
    args = parser.parse_args()
    if args.reader_only:
        raise SystemExit('CyCodeReader moved to https://github.com/MrMybal/CyCodeReader. Run python scripts/build.py in that checkout.')
    if args.test_filter and not args.run_tests:
        parser.error('--test-filter requires --run-tests')
    args.build_tests = args.build_tests or args.run_tests
    if args.plan:
        editor = args.editor_root or os.environ.get('CYCODE_EDITOR_ROOT') or ROOT / 'runtime/deps/cycode-reader'
        cmake = shutil.which('cmake') or 'cmake'
        build, commands = build_commands(args, cmake, shutil.which('ninja'), editor)
        print(json.dumps({'buildDirectory': str(build), 'commands': commands,
                          'testCommand': test_command(args, cmake, build) if args.run_tests else None,
                          'installsExecutable': not (args.core_only or args.no_install or args.build_tests)}, indent=2))
        return 0
    from editor_dependency import ensure_editor
    editor=ensure_editor(args.editor_root)
    if not args.core_only and not args.reader_only:
        from fetch_runtime_tools import ensure_runtime_tools
        ensure_runtime_tools()
        from setup_debugger import ensure_debugger
        ensure_debugger()
        from setup_csharp import ensure_csharp
        ensure_csharp()
        from setup_languages import ensure_languages
        ensure_languages()
        if shutil.which('node'):
            from setup_web import ensure_web
            ensure_web()
        else:
            print('Node.js 20+ is required for web language services; syntax coloring remains available.',flush=True)
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
    build, commands = build_commands(args, cmake, ninja, editor)
    build.mkdir(parents=True, exist_ok=True)
    log = build / 'build.log'
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
    if args.run_tests:
        # Keep owned fixtures under an identifiable workspace path, not OS temp.
        # This is not an antivirus exclusion; never restore/retry deleted binaries.
        temporary = ROOT / 'runtime/test-workspaces' / ('core' if args.core_only else 'native')
        temporary.mkdir(parents=True, exist_ok=True)
        test_environment = dict(environment, TMP=str(temporary), TEMP=str(temporary), TMPDIR=str(temporary))
        result = subprocess.run(test_command(args, cmake, build), env=test_environment, cwd=ROOT)
        if result.returncode:
            print('Native tests stopped at the first failure. No automatic rebuild or retry. '
                  'If a binary was removed, inspect the antivirus report before another native run.', file=sys.stderr)
            return result.returncode
    if not args.core_only and not args.no_install and not args.build_tests:
        for name in ['CyCode']:
            executable = name + ('.exe' if sys.platform == 'win32' else '')
            shutil.copy2(build / args.configuration / executable if sys.platform == 'win32' else build / executable, ROOT / executable)
    print(f'Build complete. Full log: {log}')
    if not args.run_tests:
        print('No native tests were executed.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())

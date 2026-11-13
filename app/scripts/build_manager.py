"""Build only the production CyUI Manager; never launch it or native tests."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cyui-source', type=Path)
    parser.add_argument('--dependencies', type=Path, default=ROOT / 'runtime/deps')
    parser.add_argument('--build', type=Path, default=ROOT / 'runtime/manager-build')
    parser.add_argument('--cmake', default='cmake')
    options = parser.parse_args()
    lock = json.loads((ROOT / 'apps/CyCodeManager/cyui-source.lock.json').read_text())
    source = (options.cyui_source or options.dependencies / ('cyui-' + lock['revision'][:12])).resolve()
    environment = {**os.environ, 'GIT_TERMINAL_PROMPT': '0', 'GIT_OPTIONAL_LOCKS': '0'}
    git = ['git', '-c', 'safe.directory=' + source.as_posix()]
    if not source.exists():
        subprocess.run([*git, 'clone', '--no-checkout', lock['repository'], str(source)], env=environment, check=True)
        subprocess.run([*git, '-C', str(source), 'checkout', '--detach', lock['revision']], env=environment, check=True)
    actual = subprocess.check_output([*git, '-C', str(source), 'rev-parse', 'HEAD'], text=True, env=environment).strip()
    dirty = subprocess.check_output([*git, '-C', str(source), 'status', '--porcelain', '--untracked-files=all'], text=True, env=environment).strip()
    if actual != lock['revision'] or dirty:
        raise ValueError('Use a clean CyUI checkout at the exact Manager revision; local changes are preserved')
    subprocess.run([options.cmake, '-S', str(ROOT / 'apps/CyCodeManager'), '-B', str(options.build),
        '-DCYUI_ROOT=' + str(source), '-DCYCODE_COMMON_DEPS=' + str(options.dependencies.resolve()),
        '-DBUILD_TESTING=OFF'], check=True)
    subprocess.run([options.cmake, '--build', str(options.build), '--config', 'Release', '--target', 'CyCodeManager'], check=True)
    print('Built CyCodeManager; no application or native test was launched.')


if __name__ == '__main__':
    main()

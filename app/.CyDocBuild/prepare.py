"""Install the locked official CyDoc package in the private documentation workspace."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

WORK = Path(__file__).resolve().parent


def prepare():
    lock = json.loads((WORK / 'cydoc.lock.json').read_text('utf-8'))
    node = shutil.which('node')
    npm = shutil.which('npm.cmd') or shutil.which('npm')
    if not node or not npm:
        raise RuntimeError('Documentation generation requires Node.js 22+ and npm')
    major = int(subprocess.check_output([node, '--version'], text=True).strip().lstrip('v').split('.')[0])
    if major < 22:
        raise RuntimeError('Documentation generation requires Node.js 22+')
    package = WORK / 'packages' / ('cydoc-' + lock['revision'][:7] + '.tgz')
    installed = WORK / 'node_modules/cydoc'
    stamp = WORK / '.cache/installed.json'
    dependency_lock = hashlib.sha256((WORK / 'package-lock.json').read_bytes()).hexdigest()
    if stamp.is_file() and package.is_file() and (installed / 'dist/viewer.js').is_file():
        recorded = json.loads(stamp.read_text('utf-8'))
        if recorded == {'revision': lock['revision'], 'packageSha256': hashlib.sha256(package.read_bytes()).hexdigest(), 'dependencyLockSha256': dependency_lock}:
            return node, installed
    source = WORK / '.cache' / ('source-' + lock['revision'][:7])
    source.parent.mkdir(parents=True, exist_ok=True)
    internal = WORK / 'internal'
    internal.mkdir(exist_ok=True)
    env = {key.upper(): value for key, value in os.environ.items()}
    env.update(GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never')
    with (internal / 'last-bootstrap.log').open('w', encoding='utf-8') as log:
        def run(args, cwd=WORK):
            subprocess.run(args, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        git = ['git', '-c', 'safe.directory=' + source.as_posix(), '-C', str(source)]
        if not source.exists():
            run(['git', '-c', 'credential.interactive=false', 'clone', '-c', 'core.autocrlf=false', '--no-checkout', lock['repository'], str(source)])
            run([*git, 'checkout', '--detach', lock['revision']])
        head = subprocess.check_output([*git, 'rev-parse', 'HEAD'], text=True, env=env).strip()
        if head != lock['revision']:
            raise RuntimeError('Preserve the cache: its revision differs from the CyDoc lock')
        if subprocess.check_output([*git, 'status', '--porcelain', '--untracked-files=no'], text=True, env=env).strip():
            raise RuntimeError('Preserve local changes to the CyDoc cache')
        if not package.is_file():
            run([npm, 'ci', '--no-audit', '--no-fund'], source)
            run([npm, 'run', 'build'], source)
            temporary = WORK / '.cache/packed'
            temporary.mkdir(exist_ok=True)
            run([npm, 'pack', '--ignore-scripts', '--pack-destination', str(temporary)], source)
            packed = temporary / ('cydoc-' + lock['version'] + '.tgz')
            package.parent.mkdir(exist_ok=True)
            shutil.copyfile(packed, package)
        run([npm, 'ci', '--omit=dev', '--no-audit', '--no-fund'])
    if json.loads((installed / 'package.json').read_text('utf-8'))['version'] != lock['version']:
        raise RuntimeError('Installed CyDoc version differs from the lock')
    if (WORK / 'node_modules/three').exists():
        raise RuntimeError('Three.js is not needed by this manual')
    stamp.parent.mkdir(exist_ok=True)
    stamp.write_text(json.dumps({'revision': lock['revision'], 'packageSha256': hashlib.sha256(package.read_bytes()).hexdigest(), 'dependencyLockSha256': dependency_lock}, indent=2) + '\n', encoding='utf-8')
    return node, installed


if __name__ == '__main__':
    node, package = prepare()
    print('Prepared official CyDoc package at revision ' + json.loads((WORK / 'cydoc.lock.json').read_text('utf-8'))['revision'])

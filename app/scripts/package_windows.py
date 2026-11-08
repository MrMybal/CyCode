"""Create an allow-listed Windows development archive, without private runtime data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from release_package import payload, write_archive, executable_version, version_tuple
from windows_installer import compile_installer, system_runtime
from verify_documentation import verify as verify_documentation

ROOT = Path(__file__).resolve().parents[2]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='Built CMake directory')
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--iscc', type=Path, help='Inno Setup 6.7+ compiler')
    parser.add_argument('--output', type=Path, default=ROOT / 'runtime/releases')
    args = parser.parse_args()
    descriptor = json.loads((ROOT / 'app/CyTool.json').read_text(encoding='utf-8'))
    version = descriptor['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise SystemExit('Invalid release version')
    build = args.build.resolve() / args.configuration
    cli = build / 'cycode_cli.exe'
    for binary in (cli, build / 'CyCode.exe', build / 'CyCodeUpdater.exe'):
        if executable_version(binary) != version_tuple(version):
            raise SystemExit('Rebuild first: executable resources differ from app/CyTool.json')
    git = ['git', '-c', f'safe.directory={ROOT.as_posix()}', '-C', str(ROOT)]
    if subprocess.check_output([*git, 'status', '--porcelain', '--untracked-files=normal'], text=True).strip():
        raise SystemExit('Release packaging requires a clean source checkout; local changes are preserved')
    revision = subprocess.check_output([*git, 'rev-parse', 'HEAD'], text=True).strip()
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH') or
                subprocess.check_output([*git, 'show', '-s', '--format=%ct', 'HEAD'], text=True).strip())
    timestamp = time.gmtime(max(epoch, 315532800))[:6]
    entries = {}

    def include(source, target=None):
        source = Path(source)
        target = target or source.relative_to(ROOT).as_posix()
        if not source.is_file() or source.is_symlink():
            raise RuntimeError(f'Missing or non-regular package file: {source}')
        parts = Path(target).parts
        if any(p in {'.git', '.CyDocBuild', 'data', 'runtime', '__pycache__'} for p in parts):
            raise RuntimeError(f'Private package path refused: {target}')
        entries[target] = source.read_bytes()

    help_root = args.build.resolve() / 'Documentation'
    if not (help_root / 'DocData/export-files.json').is_file():
        raise RuntimeError('Build the complete local documentation before packaging')
    verify_documentation(help_root)
    for source in sorted(help_root.rglob('*')):
        if source.is_file(): include(source, 'app/Documentation/' + source.relative_to(help_root).as_posix())
    include(build / 'CyCode.exe', 'CyCode.exe')
    include(cli, 'cycode_cli.exe')
    include(build / 'CyCodeUpdater.exe', 'CyCodeUpdater.exe')
    for name in ['CyCode.png', 'CyCode.ico', 'CyCode-document.png', 'CyCode-document.ico']:
        include(ROOT / 'icons' / name)
    for plugin in ['sample', 'unreal', 'svn', 'perforce', 'mercurial', 'diversion']:
        library = 'cycode_' + plugin
        include(ROOT / 'runtime/plugins' / (library + '.dll'), 'extensions/' + library + '.dll')
        include(ROOT / 'app/plugins' / plugin / 'plugin.json', 'extensions/' + plugin + '.json')
    for name in ['LICENSE', 'COPYRIGHT', 'README.md', 'README.fr.md', 'LISEZ-MOI.md',
                 'VERSION.fr.md', 'CHANGELOG.md', f'app/Docs/Releases/v{version}.md',
                 'website/assets/workspace.png', 'website/assets/git-conflicts.png', 'app/CyTool.json',
                 'app/README.md', 'app/THIRD_PARTY_NOTICES.md', 'app/tool.py', 'app/contracts.py',
                 'app/locales/ui.json', 'app/include/cycode/PluginAPI.h']:
        include(ROOT / name)
    for pattern in ['app/*lock.json', 'app/licenses/*', 'app/resources/fonts/*', 'app/scripts/*.py',
                    'app/examples/hello/*']:
        for source in sorted(ROOT.glob(pattern)):
            if source.is_file():
                if source.name in {'build_documentation.py', 'verify_documentation.py', 'cydoc_dependency.py', 'publish_documentation.py'}:
                    continue
                include(source)
    tools = ROOT / 'runtime/tools'
    stamp = json.loads((tools / 'ripgrep.json').read_text(encoding='utf-8'))
    locked = json.loads((ROOT / 'app/runtime-tools.lock.json').read_text(encoding='utf-8'))['ripgrep']
    if stamp['archiveSha256'] != locked['platforms']['windows-x64']['sha256']:
        raise RuntimeError('ripgrep archive does not match the Windows x64 lock')
    if stamp['binarySha256'] != sha256((tools / 'rg.exe').read_bytes()):
        raise RuntimeError('ripgrep executable differs from the verified installation')
    include(tools / 'rg.exe', 'tools/rg.exe')
    for name in ['COPYING', 'LICENSE-MIT', 'UNLICENSE']:
        include(tools / 'licenses/ripgrep' / name, 'tools/licenses/ripgrep/' + name)
    entries['PACKAGE.md'] = f'''# CyCode {version} — Windows x64

Extract the complete archive to a writable folder and run CyCode.exe. Keep app/,
icons/ and tools/ beside the executable. The app-local MSVC runtime is included; an OpenGL 3.3
driver is required. cycode_cli.exe --version reports the bundled version.

IDE, text/image editing, project search and native services are included. Git, SVN, Perforce, Mercurial, Diversion,
compilers/build tools and language servers/debug adapters are external tools.
This development package does not include a private Python/SDK environment.
Optional MCP setup: python app/scripts/setup_runtime.py --core-wheel PATH_TO_WHEEL.
The scripts and notices describe local installation of additional toolchains.

No settings, credentials, SDK wheels, downloaded dependency source trees or
development runtime directories are included. data/ and runtime/ are created
locally when needed. This is unsigned and has not been tested on a clean machine.
Automatic updates use stable GitHub Releases and this same portable ZIP payload.
The application checks at startup and asks before download or installation.
Close every instance of this installation before replacement. data/ and unknown
files are preserved. Local changes to package-owned files block replacement.
Private repositories require CYCODE_UPDATE_TOKEN (or GH_TOKEN/GITHUB_TOKEN) in
the environment; credentials are never stored in preferences.
Linux/macOS binaries are not provided by this release.

Source commit: {revision}
CyUI: https://github.com/MrMybal/CyUI
CyUI MIT license: app/licenses/CyUI.txt; official link in app/THIRD_PARTY_NOTICES.md.
CyCode project license: GPL-3.0-only. See LICENSE, COPYRIGHT and app/THIRD_PARTY_NOTICES.md.
Corresponding source: https://github.com/MrMybal/CyCode/tree/{revision}
Pinned dependency sources are identified in manifest.json and the app/*source.lock.json
files. Binary recipients must also have access to those sources and build scripts.
'''.encode('utf-8')
    entries.update(system_runtime(args.build.resolve(), args.configuration, args.output.resolve()))
    entries = payload(entries, version, {
        'license': 'GPL-3.0-only', 'sourceCommit': revision,
        'cyui': json.loads((ROOT / 'app/cyui-source.lock.json').read_text(encoding='utf-8')),
        'sharedEditor': json.loads((ROOT / 'app/editor-source.lock.json').read_text(encoding='utf-8'))})
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f'CyCode-{version}-windows-x64.zip'
    # The native update reader accepts stored ZIP entries. One portable archive
    # now serves downloads and in-app updates, with the same complete manifest.
    write_archive(archive, entries, version, timestamp, update=True)
    if archive.stat().st_size > 256 * 1024 * 1024:
        raise RuntimeError('Portable archive exceeds native download limit')
    installer = compile_installer(entries, version, args.output.resolve(), args.iscc)
    checksums = {file.name: sha256(file.read_bytes()) for file in (installer, archive)}
    (args.output / 'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name,digest in checksums.items()), encoding='utf-8')
    print(json.dumps({'installer': str(installer), 'archive': str(archive), 'sha256': checksums,
                      'files': len(entries), 'commit': revision}, indent=2))




if __name__ == '__main__':
    main()

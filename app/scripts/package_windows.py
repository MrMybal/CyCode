"""Create an allow-listed Windows development archive, without private runtime data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='Built CMake directory')
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--output', type=Path, default=ROOT / 'runtime/releases')
    args = parser.parse_args()
    descriptor = json.loads((ROOT / 'app/CyTool.json').read_text(encoding='utf-8'))
    version = descriptor['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise SystemExit('Invalid release version')
    build = args.build.resolve() / args.configuration
    cli = build / 'cycode_cli.exe'
    actual = subprocess.check_output([str(cli), '--version'], text=True).strip()
    if actual != 'CyCode ' + version:
        raise SystemExit('Rebuild first: CLI version differs from app/CyTool.json')
    git = ['git', '-c', f'safe.directory={ROOT.as_posix()}', '-C', str(ROOT)]
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
        if any(p in {'.git', 'data', 'runtime', '__pycache__'} for p in parts):
            raise RuntimeError(f'Private package path refused: {target}')
        entries[target] = source.read_bytes()

    include(build / 'CyCode.exe', 'CyCode.exe')
    include(cli, 'cycode_cli.exe')
    for name in ['README.md', 'LISEZ-MOI.md', 'CHANGELOG.md', 'app/CyTool.json',
                 'app/README.md', 'app/THIRD_PARTY_NOTICES.md', 'app/tool.py', 'app/contracts.py',
                 'app/locales/ui.json', 'app/include/cycode/PluginAPI.h']:
        include(ROOT / name)
    for pattern in ['app/*lock.json', 'app/Docs/*.md', 'app/licenses/*', 'app/scripts/*.py',
                    'app/examples/hello/*']:
        for source in sorted(ROOT.glob(pattern)):
            if source.is_file():
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

Extract the complete archive to a writable folder and run CyCode.exe. Keep app/
and tools/ beside the executable. Visual C++ 2022 x64 runtime and an OpenGL 3.3
driver are required. cycode_cli.exe --version reports the bundled version.

IDE, text/image editing, project search and native services are included. Git,
compilers/build tools and language servers/debug adapters are external tools.
This development package does not include a private Python/SDK environment.
Optional MCP setup: python app/scripts/setup_runtime.py --core-wheel PATH_TO_WHEEL.
The scripts and notices describe local installation of additional toolchains.

No settings, credentials, SDK wheels, downloaded dependency source trees or
development runtime directories are included. data/ and runtime/ are created
locally when needed. This is unsigned and has not been tested on a clean machine.
Linux/macOS binaries are not provided by this release.

Source commit: {revision}
CyUI: https://github.com/MrMybal/CyUI (currently private)
CyUI MIT license: app/licenses/CyUI.txt; official link in app/THIRD_PARTY_NOTICES.md.
CyCode project license: undecided. See app/THIRD_PARTY_NOTICES.md.
'''.encode('utf-8')
    manifest = {'version': version, 'platform': 'windows-x64', 'sourceCommit': revision,
                'cyui': json.loads((ROOT / 'app/cyui-source.lock.json').read_text(encoding='utf-8')),
                'sharedEditor': json.loads((ROOT / 'app/editor-source.lock.json').read_text(encoding='utf-8')),
                'files': [{'path': name, 'sha256': sha256(data)} for name, data in sorted(entries.items())]}
    entries['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode('utf-8')
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f'CyCode-{version}-windows-x64.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(f'CyCode-{version}/' + name, timestamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            package.writestr(info, data, compresslevel=9)
    checksum = sha256(archive.read_bytes())
    (args.output / 'SHA256SUMS.txt').write_text(f'{checksum}  {archive.name}\n', encoding='utf-8')
    print(json.dumps({'archive': str(archive), 'sha256': checksum,
                      'files': len(entries), 'commit': revision}, indent=2))


if __name__ == '__main__':
    main()

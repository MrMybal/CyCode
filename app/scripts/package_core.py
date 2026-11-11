"""Author a standalone Core package from an existing build, never build/run Core."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile

from mcp_bundle import load_bundle, lock_digest
from release_package import pe_machine, version_tuple
from windows_installer import find_iscc, system_runtime

ROOT = Path(__file__).resolve().parents[2]
VERSION = '0.1.2'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_file(path, limit=64 * 1024 * 1024):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError('Missing, redirected or oversized package input: ' + str(path))
    return path.read_bytes()


def collect(root, bundle, *, agents_only=False, executable=None, build=None, output=None, tools_root=None, documentation=None):
    if not agents_only and (executable is None or not executable.is_file()):
        raise ValueError('The existing production Core is missing. Packaging never rebuilds or substitutes a binary.')
    entries = {}
    names = ['LICENSE', 'COPYRIGHT', 'icons/CyCode.ico', 'icons/CyCode.png',
             'app/tool.py', 'app/core_host.py', 'app/core_updates.py', 'app/contracts.py', 'app/CyTool.json',
             'app/MCPMethods.json', 'app/requirements.txt', 'app/LICENSE-CyToolsCore.txt',
             'app/THIRD_PARTY_NOTICES.md', 'app/mcp-runtime.lock.json',
             'app/scripts/install_core_agents.py', 'apps/CyCodeUpdater/main.py', 'app/Docs/Agents.md',
             'app/Docs/CoreInstallation.md', 'app/Docs/WindowsSecurity.md',
             'app/Docs/LargeProjects.md', 'app/Docs/AnalysisProgress.md',
             'app/Docs/CoreExtraction.md', 'app/Docs/AdvancedDebugging.md', 'app/Docs/CoreQuality.md',
             'app/Docs/Benchmarks/core-capacity-20261111.json',
             'app/Docs/Benchmarks/core-delivery-20261111.json']
    for name in names:
        entries[name] = read_file(root / name)
    skills = root / 'app/skills/cycode-core'
    for source in sorted(skills.rglob('*')):
        if source.is_file():
            entries['app/skills/cycode-core/' + source.relative_to(skills).as_posix()] = read_file(source)
    for name in ('CyToolsCore.txt', 'MCP-Python.txt', 'Monocypher.txt', 'nlohmann-json.txt',
                 'PCRE2.txt', 'utf8proc.txt', 'libvterm.txt', 'TinyXML2.txt'):
        entries['app/licenses/' + name] = read_file(root / 'app/licenses' / name)
    lock = json.loads(entries['app/mcp-runtime.lock.json'])
    entries.update(load_bundle(bundle, lock))
    if not agents_only:
        if executable is None or not executable.is_file():
            raise ValueError('The existing production Core is missing. Packaging never rebuilds or substitutes a binary.')
        with executable.open('rb') as stream:
            if pe_machine(stream) != 0x8664:
                raise ValueError('A Windows x64 production Core is required')
        entries['cycode_cli.exe'] = read_file(executable)
        if build is None:
            raise ValueError('The production build directory is required for its MSVC runtime')
        entries.update(system_runtime(build, 'Release', output))
        tools = tools_root or root / 'runtime/tools'
        stamp = json.loads(read_file(tools / 'ripgrep.json'))
        locked = json.loads(read_file(root / 'app/runtime-tools.lock.json'))['ripgrep']['platforms']['windows-x64']
        binary = read_file(tools / 'rg.exe')
        if stamp['archiveSha256'] != locked['sha256'] or stamp['binarySha256'] != sha(binary):
            raise ValueError('The installed ripgrep does not match its pinned inventory')
        entries['tools/rg.exe'] = binary
        for name in ('COPYING', 'LICENSE-MIT', 'UNLICENSE'):
            entries['tools/licenses/ripgrep/' + name] = read_file(tools / 'licenses/ripgrep' / name)
        from verify_documentation import verify
        help_root = documentation or root / 'app/Documentation'
        verify(help_root)
        for source in sorted(help_root.rglob('*')):
            if source.is_file():
                entries['app/Documentation/' + source.relative_to(help_root).as_posix()] = read_file(source)
    return entries


def manifest(entries, version, revision, *, agents_only):
    version_tuple(version)
    if not revision or len(revision) != 40 or any(c not in '0123456789abcdef' for c in revision):
        raise ValueError('An exact source commit is required')
    return {'format': 'cycode-core-distribution/1', 'product': 'CyCodeCore',
            'version': version, 'channel': 'preview', 'engineVersion': json.loads(entries['app/CyTool.json'])['version'],
            'sourceRevision': revision, 'platform': 'windows-x64',
            'state': 'agents-only' if agents_only else 'complete', 'nativeExecutedByPackager': False,
            'files': [{'path': n, 'size': len(b), 'sha256': sha(b)} for n, b in sorted(entries.items())]}


def write_archive(path, entries, version):
    # The archive has one product root and only explicitly collected files.
    seen = set()
    for name in entries:
        if (name.startswith('/') or '\\' in name or ':' in name or
                any(p in {'', '.', '..', '.git', '__pycache__', 'data', 'runtime'} for p in name.split('/')) or
                name.lower() in seen):
            raise ValueError('Unsafe archive entry')
        seen.add(name.lower())
    if any(any(n[:i] in seen for i, c in enumerate(n) if c == '/') for n in seen):
        raise ValueError('File/directory archive collision')
    if len(entries) > 4096 or sum(map(len, entries.values())) > 240 * 1024 * 1024:
        raise ValueError('Core package budget exceeded')
    if path.exists():
        raise ValueError('An existing archive is preserved')
    with zipfile.ZipFile(path, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo('CyCodeCore-' + version + '/' + name, (2026, 11, 9, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)


def installer(entries, version, output, iscc=None):
    if json.loads(entries['core-distribution.json'])['state'] != 'complete':
        raise ValueError('An agents-only archive cannot be labelled as a functional installer')
    compiler = find_iscc(iscc)
    with tempfile.TemporaryDirectory(prefix='core-setup-', dir=output) as temporary:
        stage = Path(temporary) / 'files'
        for name, data in entries.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        subprocess.run([str(compiler), '/DAppVersion=' + version, '/DSourceDir=' + str(stage),
                        '/DOutputDir=' + str(output), str(ROOT / 'app/packaging/windows-core.iss')], check=True)
    return output / ('CyCodeCore-' + version + '-windows-x64-setup.exe')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default=VERSION)
    parser.add_argument('--revision', required=True, help='Exact clean source commit being packaged')
    parser.add_argument('--mcp-bundle', type=Path)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--agents-only', action='store_true', help='Explicitly omit the native Core; not an active installation')
    parser.add_argument('--installer', action='store_true')
    parser.add_argument('--iscc', type=Path)
    parser.add_argument('--tools-root', type=Path, help='Existing verified ripgrep installation; never downloaded by this command')
    parser.add_argument('--documentation', type=Path, help='Verified complete local CyDoc export')
    args = parser.parse_args()
    if args.agents_only and args.installer:
        parser.error('--installer requires the real native Core')
    git = ['git', '-c', 'safe.directory=' + ROOT.as_posix(), '-C', str(ROOT)]
    head = subprocess.check_output([*git, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output([*git, 'status', '--porcelain', '--untracked-files=normal'], text=True)
    if head != args.revision or dirty.strip():
        raise ValueError('Commit the reviewed sources first; revision must match the clean checkout being packaged')
    args.output.mkdir(parents=True, exist_ok=True)
    lock = json.loads(read_file(ROOT / 'app/mcp-runtime.lock.json'))
    bundle = args.mcp_bundle or ROOT / 'runtime/mcp-bundles' / lock_digest(lock)
    entries = collect(ROOT, bundle, agents_only=args.agents_only, executable=args.executable,
                      build=args.build, output=args.output, tools_root=args.tools_root, documentation=args.documentation)
    record = manifest(entries, args.version, args.revision, agents_only=args.agents_only)
    entries['core-distribution.json'] = (json.dumps(record, indent=2) + '\n').encode('utf-8')
    entries['INSTALL.md'] = entries['app/Docs/CoreInstallation.md']
    suffix = 'agents-only.zip' if args.agents_only else 'windows-x64.zip'
    archive = args.output / ('CyCodeCore-' + args.version + '-' + suffix)
    write_archive(archive, entries, args.version)
    results = [archive]
    if args.installer:
        results.append(installer(entries, args.version, args.output, args.iscc))
    checks = args.output / 'SHA256SUMS.txt'
    if checks.exists():
        raise ValueError('Existing checksum file preserved')
    checks.write_text(''.join(sha(p.read_bytes()) + '  ' + p.name + '\n' for p in results), encoding='utf-8')
    print(json.dumps({'state': record['state'], 'assets': [str(p) for p in results],
                      'files': len(entries), 'nativeExecuted': False}, indent=2))


if __name__ == '__main__':
    main()

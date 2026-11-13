"""Installation-owned language tools; no PATH, registry or workspace changes."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import stat
import tempfile
import zipfile
from core_installation import installation_root, management_lock
from scripts.install_core_agents import ordinary, bounded, atomic

MAX_EXPANDED = 512 * 1024 * 1024


def host_architecture():
    if os.name == 'nt':
        # MCP clients may remove PROCESSOR_ARCHITECTURE. Query Windows itself;
        # do not mistake a filtered environment for an unsupported machine.
        import ctypes
        info = ctypes.create_string_buffer(64)
        ctypes.windll.kernel32.GetNativeSystemInfo(ctypes.byref(info))
        return {9: 'AMD64', 12: 'ARM64', 0: 'x86'}.get(int.from_bytes(info.raw[:2], 'little'), '')
    return platform.machine()


def catalogue():
    value = json.loads(bounded(Path(__file__).with_name('clangd.lock.json'), 8192))
    if not isinstance(value, dict): raise ValueError('Invalid pinned clangd download')
    version = value.get('version', '')
    if (not isinstance(version, str) or not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version)
            or value.get('platform') != 'windows-x64'
            or value.get('url') != f'https://github.com/clangd/clangd/releases/download/{version}/clangd-windows-{version}.zip'
            or type(value.get('size')) is not int or not 0 < value['size'] <= 128 * 1024 * 1024
            or not isinstance(value.get('sha256'), str) or not re.fullmatch('[a-f0-9]{64}', value['sha256'])):
        raise ValueError('Invalid pinned clangd download')
    return value


def selection(root):
    folder = ordinary(root / 'dependencies/clangd')
    pointer = folder / 'current.json'
    if not pointer.is_file():
        return None
    data = json.loads(bounded(pointer, 4096))
    if (not isinstance(data, dict) or data.get('format') != 'cycode-clangd/1' or not isinstance(data.get('version'), str)
            or len(data['version']) > 64 or not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', data['version'])):
        raise ValueError('Invalid managed clangd selection preserved')
    executable = ordinary(folder / data['version'] / 'bin/clangd.exe')
    if not executable.is_file():
        raise ValueError('Selected clangd is missing; reinstall it from Dependencies')
    return {'path': str(executable), 'version': data['version'], 'source': 'managed'}


def inspect(home, *, root=None):
    root = ordinary(root or installation_root(home))
    issue = ''
    try: managed = selection(root)
    except (ValueError, OSError, KeyError) as error:
        managed, issue = None, str(error)
    candidates = [os.environ.get('CYCODE_CLANGD', ''), str(home / 'tools/clangd.exe')]
    system = shutil.which('clangd')
    if system:
        candidates.append(system)
    if os.name == 'nt':
        candidates.append(str(Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'LLVM/bin/clangd.exe'))
    override = os.environ.get('CYCODE_CLANGD', '')
    explicit = {'path': override, 'version': None, 'source': 'override'} if override and Path(override).is_absolute() and Path(override).is_file() else None
    found = explicit or managed or next(({'path': p, 'version': None, 'source': 'external'} for p in candidates
        if p and Path(p).is_absolute() and Path(p).is_file()), None)
    tools = [{'id': 'clangd', 'requiredFor': 'C/C++ semantic analysis', 'state': 'installed' if found else 'missing',
              'selection': found, 'managed': managed, 'issue': issue, 'download': catalogue(),
              'installationDirectory': str(root / 'dependencies/clangd'), 'automaticInstall': os.name == 'nt' and host_architecture().lower() in ('amd64', 'x86_64')}]
    for name, use, page, bundled in [
        ('rg', 'Project text search', 'https://github.com/BurntSushi/ripgrep/releases', home / 'tools/rg.exe'),
        ('git', 'Git repositories', 'https://git-scm.com/downloads', None),
        ('cmake', 'CMake projects', 'https://cmake.org/download/', None),
        ('dotnet', 'C#/.NET projects', 'https://dotnet.microsoft.com/download', None),
        ('clang-tidy', 'C++ inspections', 'https://releases.llvm.org/', None),
        ('lldb-dap', 'Native debugging', 'https://releases.llvm.org/', None)]:
        path = str(bundled) if bundled and bundled.is_file() else shutil.which(name)
        if not path and os.name == 'nt' and name in ('clang-tidy', 'lldb-dap'):
            candidate = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'LLVM/bin' / (name + '.exe')
            if candidate.is_file(): path = str(candidate)
        if not path and os.name == 'nt':
            programs = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))
            hints = {'git': [programs / 'Git/cmd/git.exe'], 'dotnet': [programs / 'dotnet/dotnet.exe'],
                     'cmake': [programs / 'CMake/bin/cmake.exe', *[programs / 'Microsoft Visual Studio/2022' / edition /
                         'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
                         for edition in ('Community', 'Professional', 'Enterprise', 'BuildTools')]]}.get(name, [])
            path = next((str(p) for p in hints if p.is_file()), None)
        tools.append({'id': name, 'requiredFor': use, 'state': 'installed' if path else 'missing',
                      'selection': {'path': path} if path else None, 'source': page, 'automaticInstall': False})
    return tools


def unpack(archive, stage, *, cancelled=lambda: False):
    """Validate the entire archive before extracting into a new owned stage."""
    stage = ordinary(stage)
    with zipfile.ZipFile(archive) as source:
        entries = source.infolist()
        if len(entries) > 8192 or sum(i.file_size for i in entries) > MAX_EXPANDED:
            raise ValueError('clangd archive exceeds the installation budget')
        roots, seen, files_seen, planned = set(), set(), set(), []
        for item in entries:
            # ZipInfo normalizes Windows separators and truncates NULs in its
            # filename. Validate the original archive spelling before that.
            name = item.orig_filename
            parts = PurePosixPath(name).parts
            if (not parts or name.startswith('/') or '\\' in name or ':' in name
                    or any(p in ('', '.', '..') or p.endswith((' ', '.'))
                           or any(ord(c) < 32 or c in '<>"|?*' for c in p)
                           or p.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL',
                               *('COM' + str(n) for n in range(1, 10)), *('LPT' + str(n) for n in range(1, 10))}
                           for p in name.rstrip('/').split('/'))
                    or stat.S_IFMT(item.external_attr >> 16) not in (0, stat.S_IFREG, stat.S_IFDIR)):
                raise ValueError('Unsafe clangd archive entry')
            roots.add(parts[0])
            relative = Path(*parts[1:])
            if len(parts) == 1:
                if not item.is_dir(): raise ValueError('Unexpected clangd archive root')
                continue
            key = relative.as_posix().casefold()
            if key in seen: raise ValueError('Duplicate clangd archive path')
            seen.add(key)
            if not item.is_dir(): files_seen.add(key)
            planned.append((item, relative))
        if len(roots) != 1 or not any(p.as_posix() == 'bin/clangd.exe' for i, p in planned):
            raise ValueError('A complete Windows clangd archive is required')
        if any(k[:n] in files_seen for k in seen for n, char in enumerate(k) if char == '/'):
            raise ValueError('Archive file/directory collision')
        files = []
        for item, relative in planned:
            if cancelled(): raise RuntimeError('clangd installation cancelled')
            target = ordinary(stage / relative)
            if item.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open(item) as incoming, target.open('xb') as output:
                shutil.copyfileobj(incoming, output, 256 * 1024)
            with target.open('rb') as stream:
                files.append({'path': relative.as_posix(), 'size': item.file_size,
                              'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()})
        return files


def install(home, *, root=None, accepted=False, cancelled=lambda: False, progress=None):
    if not accepted:
        raise ValueError('Review the source, license and destination before installing clangd')
    if os.name != 'nt' or host_architecture().lower() not in ('amd64', 'x86_64'):
        raise ValueError('Managed installation currently supports Windows x64 only')
    lock = catalogue()
    if cancelled(): raise RuntimeError('clangd installation cancelled')
    root = ordinary(root or installation_root(home))
    folder = ordinary(root / 'dependencies/clangd')
    with management_lock(folder):
        destination = ordinary(folder / lock['version'])
        if not destination.exists():
            # Real CyToolsCore downloader: bounded known size, resumable transfer,
            # SHA-256 validation and its own cancellation/progress implementation.
            from cytools_core.downloads import DownloadManager
            class Cancellation:
                def check(self):
                    if cancelled(): raise RuntimeError('clangd download cancelled')
            download = DownloadManager(ordinary(folder / 'downloads'), use_shared_cache=False).download(
                'clangd-' + lock['version'] + '.zip', lock['url'], sha256=lock['sha256'], size=lock['size'],
                cancellation=Cancellation(),
                progress=(lambda received, total: progress({'phase': 'download', 'downloaded': received, 'total': total})) if progress else None)
            if download['state'] != 'Completed': raise RuntimeError('clangd download did not complete')
            if shutil.disk_usage(folder).free < MAX_EXPANDED + lock['size']:
                raise ValueError('Not enough free space to stage clangd safely')
            stage = Path(tempfile.mkdtemp(prefix='.clangd-', dir=folder))
            try:
                files = unpack(download['path'], stage, cancelled=cancelled)
                atomic(stage / 'cycode-dependency.json', json.dumps({'format': 'cycode-clangd-files/1',
                    'version': lock['version'], 'archiveSha256': lock['sha256'], 'files': files}, indent=2).encode(), None)
                Cancellation().check()
                os.rename(stage, destination)  # Never overwrite a running/older version.
            finally:
                if stage.exists():
                    resolved = stage.resolve()
                    if not resolved.is_relative_to(folder.resolve()) or not stage.name.startswith('.clangd-'):
                        raise ValueError('Unsafe clangd staging cleanup refused')
                    shutil.rmtree(resolved)
        receipt = json.loads(bounded(destination / 'cycode-dependency.json', 4 * 1024 * 1024))
        if (receipt.get('format') != 'cycode-clangd-files/1' or receipt.get('version') != lock['version']
                or receipt.get('archiveSha256') != lock['sha256'] or not isinstance(receipt.get('files'), list)
                or not any(r.get('path') == 'bin/clangd.exe' for r in receipt['files'])):
            raise ValueError('Unowned clangd installation preserved')
        for row in receipt['files']:
            file = ordinary(destination / row['path'])
            if not file.resolve().is_relative_to(destination.resolve()) or file.stat().st_size != row['size']:
                raise ValueError('Managed clangd file changed')
            with file.open('rb') as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != row['sha256']:
                    raise ValueError('Managed clangd file changed')
        pointer = folder / 'current.json'
        previous = bounded(pointer, 4096) if pointer.exists() else None
        atomic(pointer, json.dumps({'format': 'cycode-clangd/1', 'version': lock['version']}).encode(), previous)
        return {**selection(root), 'installedBytes': sum(r['size'] for r in receipt['files']),
                'reloadRequired': True, 'systemPathChanged': False}

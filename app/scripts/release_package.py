"""Pure release authoring/data helpers; never execute packaged application code."""
import ctypes
import hashlib
import json
import re
import struct
import zipfile

MAXIMUM = 256 * 1024 * 1024
REQUIRED = {'CyCode.exe', 'CyCodeUpdater.exe', 'cycode_cli.exe',
            'app/CyTool.json', 'app/locales/ui.json', 'LICENSE', 'COPYRIGHT'}
REQUIRED.update('tools/python/' + name for name in (
    'python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt','cycode-runtime.json',
    'Lib/site-packages/cytools_core/__init__.py','Lib/site-packages/mcp/__init__.py'))
REQUIRED.update('app/resources/fonts/' + name for name in (
    'Roboto-Regular.ttf','Roboto-Bold.ttf','JetBrainsMono-Regular.ttf',
    'CyCodeControls-Regular.ttf','font-resources.json','Roboto-LICENSE.txt',
    'JetBrainsMono-OFL.txt','Codicons-LICENSE.txt'))


def version_tuple(value):
    if not isinstance(value, str) or not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', value):
        raise ValueError('Invalid release version')
    result = tuple(map(int, value.split('.')))
    if max(result) > 65535:
        raise ValueError('Release version exceeds native version limits')
    return result


def package_path(name):
    if not isinstance(name, str) or not name or len(name) > 512:
        return False
    for part in name.split('/'):
        key = part.lower()
        stem = key.split('.')[0]
        if (not re.fullmatch(r'[A-Za-z0-9_.-]+', part) or part in {'.', '..'} or
                part.endswith('.') or key in {'data', 'runtime', '.git', '.cydocbuild'} or
                stem in {'con', 'nul', 'prn', 'aux'} or re.fullmatch(r'(com|lpt)[1-9]', stem)):
            return False
    return (name in {'CyCode.exe', 'CyCodeUpdater.exe', 'cycode_cli.exe', 'manifest.json',
                     'PACKAGE.md', 'README.md', 'README.fr.md', 'VERSION.fr.md', 'LISEZ-MOI.md', 'CHANGELOG.md', 'LICENSE', 'COPYRIGHT',
                     'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll', 'msvcp140_atomic_wait.dll',
                     'msvcp140_codecvt_ids.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'concrt140.dll', 'website/assets/workspace.png', 'website/assets/git-conflicts.png'} or
            name.startswith(('app/', 'icons/', 'tools/', 'extensions/')))


def payload(entries, version, provenance):
    version_tuple(version)
    entries = dict(entries)
    if 'manifest.json' in entries or len(entries) > 4095 or not REQUIRED.issubset(entries):
        raise ValueError('Incomplete or duplicate release payload')
    unique = set()
    for name, data in entries.items():
        if not package_path(name) or name.lower() in unique or not isinstance(data, bytes) or len(data) > 64 * 1024 * 1024:
            raise ValueError(f'Unsafe, duplicate or oversized release entry: {name}')
        unique.add(name.lower())
    for name in unique:
        if any(name[:i] in unique for i, char in enumerate(name) if char == '/'):
            raise ValueError('Conflicting release paths')
    if sum(map(len, entries.values())) > 240 * 1024 * 1024:
        raise ValueError('Release payload exceeds update extraction budget')
    manifest = dict(provenance, version=version, platform='windows-x64', updateFormat=1,
                    files=[dict(path=name, size=len(data), sha256=hashlib.sha256(data).hexdigest())
                           for name, data in sorted(entries.items())])
    encoded = (json.dumps(manifest, indent=2) + '\n').encode('utf-8')
    if len(encoded) > 2 * 1024 * 1024:
        raise ValueError('Release manifest exceeds update limit')
    entries['manifest.json'] = encoded
    return entries


def write_archive(destination, entries, version, timestamp, *, update):
    version_tuple(version)
    compression = zipfile.ZIP_STORED if update else zipfile.ZIP_DEFLATED
    with zipfile.ZipFile(destination, 'w', compression=compression, allowZip64=False) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(f'CyCode-{version}/' + name, timestamp)
            info.compress_type = compression
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data, compresslevel=None if update else 9)


def pe_machine(stream):
    """Read a PE/COFF executable header from a seekable stream, without loading it."""
    stream.seek(0)
    header = stream.read(64)
    if len(header) != 64 or header[:2] != b'MZ':
        raise ValueError('Invalid executable DOS header')
    offset = struct.unpack_from('<I', header, 60)[0]
    if offset < 64 or offset > 16 * 1024 * 1024:
        raise ValueError('Invalid executable PE offset')
    stream.seek(offset)
    pe = stream.read(26)
    if len(pe) != 26 or pe[:4] != b'PE\x00\x00':
        raise ValueError('Invalid executable PE header')
    machine, optional_size, flags, magic = (struct.unpack_from('<H', pe, at)[0] for at in (4, 20, 22, 24))
    if optional_size < 2 or not flags & 2 or flags & 0x2000 or magic != 0x20b:
        raise ValueError('Expected a PE32+ executable')
    return machine


def executable_version(path):
    """Read Windows PE resources through system APIs, without launching the EXE."""
    with path.open('rb') as stream:
        if pe_machine(stream) != 0x8664:
            raise ValueError('Windows x64 packages require AMD64 executables')
    from ctypes import wintypes
    library = ctypes.WinDLL('version', winmode=0x00000800)  # System32 only
    library.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
    library.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    library.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    library.GetFileVersionInfoW.restype = wintypes.BOOL
    library.VerQueryValueW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT)]
    library.VerQueryValueW.restype = wintypes.BOOL
    size = library.GetFileVersionInfoSizeW(str(path), None)
    if not size:
        raise ValueError(f'Missing executable version resource: {path.name}')
    data = ctypes.create_string_buffer(size)
    if not library.GetFileVersionInfoW(str(path), 0, size, data):
        raise ValueError('Cannot read executable version resource')
    pointer, length = ctypes.c_void_p(), wintypes.UINT()
    if not library.VerQueryValueW(data, '\\', ctypes.byref(pointer), ctypes.byref(length)) or length.value < 52:
        raise ValueError('Invalid executable version resource')
    fields = ctypes.cast(pointer, ctypes.POINTER(wintypes.DWORD))
    if fields[0] != 0xFEEF04BD:
        raise ValueError('Invalid Windows version signature')
    return fields[2] >> 16, fields[2] & 65535, fields[3] >> 16

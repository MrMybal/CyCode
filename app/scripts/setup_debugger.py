"""Install checksum-pinned CodeLLDB and NetCoreDbg adapters under CyCode only."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import sys
import tarfile
import urllib.request
import zipfile
ROOT = Path(__file__).resolve().parents[2]
def ensure_codelldb():
    lock = json.loads((ROOT/'app/debugger-tools.lock.json').read_text('utf-8'))['codelldb']
    arch = 'arm64' if platform.machine().lower() in ('arm64','aarch64') else 'x64'
    target = f'{sys.platform}-{arch}'
    if target not in lock['platforms']:
        print(f'No bundled CodeLLDB for {target}; configure an installed DAP adapter.',flush=True)
        return
    destination = ROOT/'runtime/tools/codelldb'
    stamp = destination/'installed.json'
    binary = destination/'extension/adapter'/('codelldb.exe' if sys.platform=='win32' else 'codelldb')
    expected = lock['platforms'][target]
    if stamp.exists() and binary.is_file() and (sys.platform=='win32' or os.access(binary,os.X_OK)):
        installed = json.loads(stamp.read_text('utf-8'))
        if installed.get('sha256') == expected and installed.get('binarySha256') == hashlib.sha256(binary.read_bytes()).hexdigest(): return
    archive = ROOT/f'runtime/tools/codelldb-{target}.vsix'
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        url = f'https://github.com/vadimcn/codelldb/releases/download/v{lock["version"]}/codelldb-{target}.vsix'
        request = urllib.request.Request(url,headers={'User-Agent':'CyCode-build/0.1'})
        with urllib.request.urlopen(request,timeout=120) as response: data = response.read(200*1024*1024)
        if hashlib.sha256(data).hexdigest() != expected: raise RuntimeError('CodeLLDB archive checksum mismatch')
        archive.parent.mkdir(parents=True,exist_ok=True)
        archive.write_bytes(data)
    with zipfile.ZipFile(archive) as package:
        for entry in package.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in entry.filename: raise RuntimeError('Unsafe CodeLLDB archive path')
            resolved = (destination/entry.filename).resolve()
            if not resolved.is_relative_to(destination.resolve()): raise RuntimeError('CodeLLDB extraction escapes its runtime directory')
        package.extractall(destination)
        for entry in package.infolist():
            if entry.external_attr >> 16 & 0o111 and not entry.is_dir(): (destination/entry.filename).chmod(0o755)
    if not binary.exists(): raise RuntimeError('CodeLLDB executable missing from archive')
    stamp.write_text(json.dumps({'version':lock['version'],'sha256':expected,'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n','utf-8')
    print(f'Installed standalone CodeLLDB {lock["version"]}',flush=True)
def ensure_netcoredbg():
    lock = json.loads((ROOT/'app/debugger-tools.lock.json').read_text('utf-8'))['netcoredbg']
    arch = 'arm64' if platform.machine().lower() in ('arm64','aarch64') else 'x64'
    target = f'{sys.platform}-{arch}'
    if target not in lock['platforms']:
        print(f'No official NetCoreDbg archive for {target}; configure an installed managed adapter.',flush=True)
        return
    expected = lock['platforms'][target]
    destination = ROOT/'runtime/tools/netcoredbg'
    binary = destination/'netcoredbg'/('netcoredbg.exe' if sys.platform=='win32' else 'netcoredbg')
    stamp = destination/'installed.json'
    if stamp.exists() and binary.is_file() and (sys.platform=='win32' or os.access(binary,os.X_OK)):
        installed = json.loads(stamp.read_text('utf-8'))
        if installed.get('sha256') == expected['sha256'] and installed.get('binarySha256') == hashlib.sha256(binary.read_bytes()).hexdigest():
            return
    archive = ROOT/f'runtime/tools/netcoredbg-{target}{".zip" if expected["url"].endswith(".zip") else ".tar.gz"}'
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != expected['sha256']:
        request = urllib.request.Request(expected['url'],headers={'User-Agent':'CyCode-build/0.1'})
        with urllib.request.urlopen(request,timeout=90) as response: data = response.read(64*1024*1024)
        if hashlib.sha256(data).hexdigest() != expected['sha256']:
            raise RuntimeError('NetCoreDbg archive checksum mismatch')
        archive.parent.mkdir(parents=True,exist_ok=True)
        archive.write_bytes(data)
    if archive.suffix == '.zip':
        with zipfile.ZipFile(archive) as package:
            for entry in package.infolist():
                if '\\' in entry.filename or not (destination/entry.filename).resolve().is_relative_to(destination.resolve()):
                    raise RuntimeError('Unsafe NetCoreDbg archive path')
            package.extractall(destination)
            if sys.platform!='win32':
                for entry in package.infolist():
                    if entry.external_attr >> 16 & 0o111 and not entry.is_dir():
                        (destination/entry.filename).chmod(0o755)
    else:
        with tarfile.open(archive) as package:
            package.extractall(destination,filter='data')
    if not binary.is_file(): raise RuntimeError('NetCoreDbg executable is missing')
    if sys.platform!='win32' and not os.access(binary,os.X_OK):
        # Some ZIP authors omit Unix mode metadata. This is the exact executable
        # from the verified official archive, never an installed system tool.
        binary.chmod(binary.stat().st_mode | 0o111)
    stamp.write_text(json.dumps({'version':lock['version'],'sha256':expected['sha256'],'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n','utf-8')
    print(f'Installed NetCoreDbg {lock["version"]}',flush=True)

def ensure_debugger():
    ensure_codelldb()
    ensure_netcoredbg()

if __name__ == '__main__': ensure_debugger()

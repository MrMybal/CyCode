"""Install a pinned C# server locally. Never installs or modifies a global SDK/tool."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import platform
import sys
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def ensure_dotnet_sdk(lock):
    system = {'win32':'win','linux':'linux','darwin':'osx'}.get(sys.platform)
    arch = 'arm64' if platform.machine().lower() in ('arm64','aarch64') else 'x64'
    target = f'{system}-{arch}'
    if target not in lock['platforms']:
        raise RuntimeError(f'No pinned private C# SDK for {target}')
    expected = lock['platforms'][target]
    destination = ROOT/'runtime/tools/csharp-dotnet'
    binary = destination/('dotnet.exe' if sys.platform=='win32' else 'dotnet')
    stamp = destination/'installed.json'
    if stamp.exists() and binary.exists():
        installed = json.loads(stamp.read_text('utf-8'))
        if installed.get('sha512') == expected['hash'] and installed.get('binarySha256') == hashlib.sha256(binary.read_bytes()).hexdigest():
            return
    archive = ROOT/'runtime/tools'/Path(expected['url']).name
    if not archive.exists() or hashlib.sha512(archive.read_bytes()).hexdigest() != expected['hash']:
        print(f'Downloading private .NET SDK {lock["version"]} for C# analysis...',flush=True)
        with urllib.request.urlopen(expected['url'],timeout=120) as response:
            data = response.read(500*1024*1024)
        if hashlib.sha512(data).hexdigest() != expected['hash']:
            raise RuntimeError('Private .NET SDK checksum mismatch')
        archive.parent.mkdir(parents=True,exist_ok=True)
        archive.write_bytes(data)
    if archive.suffix == '.zip':
        with zipfile.ZipFile(archive) as package:
            for entry in package.infolist():
                if '\\' in entry.filename or not (destination/entry.filename).resolve().is_relative_to(destination.resolve()):
                    raise RuntimeError('Unsafe private .NET SDK path')
            package.extractall(destination)
    else:
        with tarfile.open(archive) as package:
            package.extractall(destination,filter='data')
    if not binary.exists():
        raise RuntimeError('Private .NET host missing from SDK archive')
    stamp.write_text(json.dumps({'version':lock['version'],'sha512':expected['hash'],'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n','utf-8')
    print(f'Installed private .NET SDK {lock["version"]}',flush=True)

def ensure_csharp():
    tools = json.loads((ROOT/'app/language-tools.lock.json').read_text('utf-8'))
    ensure_dotnet_sdk(tools['dotnet-sdk'])
    lock = tools['csharp-ls']
    destination = ROOT/'runtime/tools/csharp-ls'
    archive = ROOT/f'runtime/tools/csharp-ls.{lock["version"]}.nupkg'
    binary = destination/f'tools/{lock["framework"]}/any/CSharpLanguageServer.dll'
    stamp = destination/'installed.json'
    if stamp.exists() and binary.exists():
        installed = json.loads(stamp.read_text('utf-8'))
        if installed.get('sha256') == lock['sha256'] and installed.get('binarySha256') == hashlib.sha256(binary.read_bytes()).hexdigest():
            return
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != lock['sha256']:
        with urllib.request.urlopen(lock['url'], timeout=90) as response:
            data = response.read(64*1024*1024)
        if hashlib.sha256(data).hexdigest() != lock['sha256']:
            raise RuntimeError('C# server package checksum mismatch')
        archive.parent.mkdir(parents=True, exist_ok=True)
        archive.write_bytes(data)
    with zipfile.ZipFile(archive) as package:
        for entry in package.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in entry.filename or not (destination/entry.filename).resolve().is_relative_to(destination.resolve()):
                raise RuntimeError('Unsafe C# server package path')
        package.extractall(destination)
    if not binary.exists():
        raise RuntimeError('C# server assembly is missing')
    stamp.write_text(json.dumps({'version':lock['version'], 'sha256':lock['sha256'], 'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest()}, indent=2)+'\n', 'utf-8')
    print(f'Installed C# language server {lock["version"]}', flush=True)

if __name__ == '__main__':
    ensure_csharp()

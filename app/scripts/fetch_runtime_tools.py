"""Install pinned runtime tools under CyCode, independently of the caller's PATH."""
import hashlib
import io
import json
from pathlib import Path
import platform
import sys
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def ensure_runtime_tools():
    system = {'win32': 'windows', 'linux': 'linux', 'darwin': 'macos'}[sys.platform]
    machine = platform.machine().lower()
    arch = 'arm64' if machine in ('aarch64', 'arm64') else 'x64'
    lock = json.loads((ROOT / 'app/runtime-tools.lock.json').read_text('utf-8'))['ripgrep']
    entry = lock['platforms'][f'{system}-{arch}']
    destination = ROOT / 'runtime/tools'
    executable = destination / ('rg.exe' if sys.platform == 'win32' else 'rg')
    stamp = destination / 'ripgrep.json'
    if executable.exists() and stamp.exists():
        current = json.loads(stamp.read_text('utf-8'))
        if current.get('archiveSha256') == entry['sha256'] and current.get('binarySha256') == hashlib.sha256(executable.read_bytes()).hexdigest():
            return
    request = urllib.request.Request(entry['url'], headers={'User-Agent': 'CyCode-build/0.1'})
    with urllib.request.urlopen(request, timeout=90) as response:
        data = response.read(32 * 1024 * 1024)
    if hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise RuntimeError('ripgrep download checksum mismatch')
    destination.mkdir(parents=True, exist_ok=True)
    licenses = destination / 'licenses/ripgrep'
    licenses.mkdir(parents=True, exist_ok=True)
    wanted = {executable.name, 'COPYING', 'LICENSE-MIT', 'UNLICENSE'}
    contents = {}
    if entry['url'].endswith('.zip'):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for member in archive.infolist():
                name = Path(member.filename).name
                if name in wanted and not member.is_dir(): contents[name] = archive.read(member)
    else:
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
            for member in archive.getmembers():
                name = Path(member.name).name
                if name in wanted and member.isfile(): contents[name] = archive.extractfile(member).read()
    if executable.name not in contents: raise RuntimeError('ripgrep archive has no executable')
    executable.write_bytes(contents.pop(executable.name))
    executable.chmod(0o755)
    for name, content in contents.items(): (licenses / name).write_bytes(content)
    stamp.write_text(json.dumps({'version': lock['version'], 'archiveSha256': entry['sha256'], 'binarySha256': hashlib.sha256(executable.read_bytes()).hexdigest()}, indent=2)+'\n', encoding='utf-8')
    print(f'Installed ripgrep {lock["version"]} in runtime/tools', flush=True)

if __name__ == '__main__':
    ensure_runtime_tools()

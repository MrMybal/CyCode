"""Fetch build-only dependencies into runtime/deps (never into the SDKs)."""
import hashlib
import argparse
import json
from pathlib import Path
import tarfile
import urllib.request
import subprocess
import sys
from editor_dependency import ensure_editor
from cyui_dependency import resolve as resolve_cyui

ROOT = Path(__file__).resolve().parents[2]
DEPS = ROOT / 'runtime' / 'deps'


def download(url, destination):
    request = urllib.request.Request(url, headers={'User-Agent': 'CyCode-build/0.1'})
    with urllib.request.urlopen(request, timeout=120) as response:
        destination.write_bytes(response.read())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--core-only', action='store_true')
    parser.add_argument('--editor-root', type=Path, help='Development checkout of CyCodeReader (default: pinned dependency)')
    parser.add_argument('--cyui', type=Path, help='Clean CyUI checkout matching the app source lock')
    options = parser.parse_args()
    editor=ensure_editor(options.editor_root)
    DEPS.mkdir(parents=True, exist_ok=True)
    lock_path = ROOT / 'app' / 'dependencies.lock.json'
    # Consume the provider's archive lock, not its application fetch entry point:
    # that would also resolve Reader's older standalone CyUI lock unnecessarily.
    lock = {**json.loads((editor/'dependencies.lock.json').read_text('utf-8')),
            **json.loads(lock_path.read_text('utf-8'))}
    for key, item in lock.items():
        if options.core_only and key not in ('json', 'vterm'):
            continue
        target = DEPS / key
        if (target / '.cycode-dependency').exists():
            if (target / '.cycode-dependency').read_text('utf-8') != item['sha256']:
                raise RuntimeError(f'{key}: cached revision differs from the dependency lock')
            print(f'{key}: cached', flush=True)
            continue
        archive = DEPS / (key + '.tar.gz')
        download(item['url'], archive)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if 'sha256' in item and item['sha256'] != digest:
            raise RuntimeError(f'{key}: checksum mismatch')
        item['sha256'] = digest
        target.mkdir(exist_ok=True)
        with tarfile.open(archive) as package:
            for member in package.getmembers():
                parts = Path(member.name).parts
                if len(parts) < 2:
                    continue
                member.name = str(Path(*parts[1:]))
                package.extract(member, target, filter='data')
        (target / '.cycode-dependency').write_text(digest, encoding='utf-8')
        print(f'{key}: {item["revision"]} sha256={digest}', flush=True)
    if not options.core_only:
        print(f'CyUI: {resolve_cyui(DEPS/"cyui-ff023e6", options.cyui)}', flush=True)


if __name__ == '__main__':
    main()

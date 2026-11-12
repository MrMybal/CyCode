"""Versioned Core installations: verify first, then atomically select a payload.

Activation never modifies, replaces or terminates an older running payload.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re

from scripts.install_core_agents import atomic, bounded, ordinary

MAX_FILES = 4096
MAX_TOTAL = 240 * 1024 * 1024
GENERATION = re.compile(r'[0-9]+\.[0-9]+\.[0-9]+-[a-fA-F0-9]{32}')


@contextmanager
def management_lock(folder):
    """An OS-owned lock, released on process exit; no stale lock-file deletion."""
    folder = ordinary(folder)
    folder.mkdir(parents=True, mode=0o700, exist_ok=True)
    path = ordinary(folder / 'management.lock')
    with path.open('a+b') as stream:
        if path.stat().st_nlink != 1:
            raise ValueError('Redirected management lock refused')
        if path.stat().st_size == 0:
            stream.write(b'\0')
            stream.flush()
        stream.seek(0)
        if os.name == 'nt':
            import msvcrt
            try:
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise ValueError('Another Core manager operation is active; retry after it finishes') from error
            try:
                yield
            finally:
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            try:
                yield
            finally:
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def version(value):
    if not isinstance(value, str) or not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', value):
        raise ValueError('Invalid Core distribution version')
    return tuple(map(int, value.split('.')))


def manifest(home):
    home = ordinary(home)
    data = bounded(home / 'core-distribution.json', 2 * 1024 * 1024)
    record = json.loads(data)
    if (not isinstance(record, dict) or record.get('format') != 'cycode-core-distribution/1' or record.get('product') != 'CyCodeCore'
            or record.get('state') != 'complete'):
        raise ValueError('A complete CyCodeCore payload is required')
    version(record.get('version'))
    revision = record.get('sourceRevision')
    if not isinstance(revision, str) or not re.fullmatch('[a-f0-9]{40}', revision):
        raise ValueError('Invalid Core source revision')
    return record, hashlib.sha256(data).hexdigest()


def installation_root(home):
    home = ordinary(home).resolve(strict=True)
    if home.parent.name.lower() == 'versions' and (home / 'core-distribution.json').is_file():
        return ordinary(home.parent.parent)
    return home


def pointer_parts(record):
    if not isinstance(record, dict):
        raise ValueError('Invalid Core installation pointer')
    relative = record.get('payload')
    if (record.get('format') != 'cycode-core-installation/1' or record.get('product') != 'CyCodeCore'
            or not isinstance(relative, str) or '\\' in relative):
        raise ValueError('Invalid Core installation pointer')
    parts = PurePosixPath(relative).parts
    if len(parts) != 2 or parts[0] != 'versions' or not GENERATION.fullmatch(parts[1]):
        raise ValueError('Invalid Core payload location')
    version(record.get('version'))
    fingerprint = record.get('manifestSha256')
    if not isinstance(fingerprint, str) or not re.fullmatch('[a-f0-9]{64}', fingerprint):
        raise ValueError('Invalid Core manifest fingerprint')
    return parts


def current_payload(root):
    root = ordinary(root).resolve(strict=True)
    pointer = root / 'current.json'
    if not pointer.exists():
        manifest(root)  # Compatibility with flat portable/legacy installations.
        return root
    record = json.loads(bounded(pointer, 8192))
    parts = pointer_parts(record)
    payload = ordinary(root.joinpath(*parts)).resolve(strict=True)
    if payload.parent != ordinary(root / 'versions').resolve(strict=True):
        raise ValueError('Core payload escaped its installation')
    data, digest = manifest(payload)
    if data['version'] != record.get('version') or digest != record.get('manifestSha256'):
        raise ValueError('Core installation pointer and payload disagree')
    return payload


def verify_payload(home, expected_version=None):
    home = ordinary(home).resolve(strict=True)
    data, digest = manifest(home)
    if expected_version is not None and data['version'] != expected_version:
        raise ValueError('The installed Core is not the requested version')
    rows = data.get('files')
    if not isinstance(rows, list) or not 1 <= len(rows) <= MAX_FILES:
        raise ValueError('Invalid Core file inventory')
    seen, total = set(), 0
    for row in rows:
        name = row.get('path') if isinstance(row, dict) else None
        if (not isinstance(name, str) or not name or name.startswith('/') or '\\' in name or ':' in name
                or any(part in {'', '.', '..', '.git', 'data', 'runtime', '__pycache__'} for part in name.split('/'))
                or name.casefold() in seen):
            raise ValueError('Unsafe or duplicate Core inventory entry')
        seen.add(name.casefold())
        size, expected = row.get('size'), row.get('sha256')
        if type(size) is not int or not 0 <= size <= 64 * 1024 * 1024 or not isinstance(expected, str) or not re.fullmatch('[a-f0-9]{64}', expected):
            raise ValueError('Invalid Core file fingerprint')
        total += size
        if total > MAX_TOTAL:
            raise ValueError('Core payload exceeds its size budget')
        path = ordinary(home / name)
        if not path.is_file() or path.stat().st_size != size or path.stat().st_nlink != 1:
            raise ValueError('Missing, redirected or changed Core file: ' + name)
        sha = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                sha.update(chunk)
        if sha.hexdigest() != expected:
            raise ValueError('Core file verification failed: ' + name)
    if not {'cycode_cli.exe', 'tools/python/python.exe', 'tools/python/pythonw.exe', 'app/tool.py',
            'app/core_host.py', 'app/skills/cycode-core/skill.md'} <= seen:
        raise ValueError('Incomplete Core inventory')
    return data, digest


def activate(root, payload, expected_version):
    with management_lock(ordinary(root) / 'installation-state'):
        return _activate(root, payload, expected_version)


def _activate(root, payload, expected_version):
    root = ordinary(root).resolve(strict=True)
    payload = ordinary(payload).resolve(strict=True)
    if payload.parent != ordinary(root / 'versions').resolve(strict=True) or not GENERATION.fullmatch(payload.name):
        raise ValueError('Activation requires a fresh versioned payload')
    data, digest = verify_payload(payload, expected_version)
    if not payload.name.startswith(data['version'] + '-'):
        raise ValueError('Core generation name does not match its distribution')
    pointer = ordinary(root / 'current.json')
    before = bounded(pointer, 8192) if pointer.exists() else None
    previous = json.loads(before) if before is not None else None
    if before is not None:
        # A recognised pointer can be repaired with a fully verified new payload
        # even if someone removed an old payload. Unknown metadata is preserved.
        pointer_parts(previous)
    if previous and version(previous['version']) > version(data['version']):
        raise ValueError('An older installer cannot replace the selected newer Core')
    result = {'format': 'cycode-core-installation/1', 'product': 'CyCodeCore',
              'version': data['version'], 'payload': 'versions/' + payload.name,
              'manifestSha256': digest, 'sourceRevision': data['sourceRevision']}
    if previous:
        # Keep rollback information without copying private workspace/job data.
        result['previousPayload'] = previous['payload']
    atomic(pointer, (json.dumps(result, indent=2) + '\n').encode('utf-8'), before)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args(argv)
    print(json.dumps(activate(args.root, args.payload, args.version), indent=2))


if __name__ == '__main__':
    main()

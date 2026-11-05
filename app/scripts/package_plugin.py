"""Create a signed local native-plugin package; never loads or executes its payload."""
import argparse
import getpass
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat

DOMAIN = b'CyCode plugin package v1\n'
MAX_FILE = 64 * 1024 * 1024
MAX_TOTAL = 256 * 1024 * 1024


def portable(name):
    path = PurePosixPath(name)
    if (not name or len(name.encode('utf-8')) > 1024 or path.is_absolute()
            or path.as_posix() != name or any(ord(c) < 32 or ord(c) == 127 for c in name)
            or any(c in name for c in '\\:<>"|?*')):
        raise ValueError('Expected a portable relative payload path')
    if len(path.parts) > 32:
        raise ValueError('Payload path depth exceeds 32 components')
    for part in path.parts:
        stem = part.split('.')[0].lower()
        if (part in ('.', '..') or len(part.encode('utf-8')) > 160 or part.endswith(('.', ' '))
                or stem in {'con', 'prn', 'aux', 'nul', 'conin$', 'conout$'}
                or re.fullmatch(r'(com|lpt)[1-9]', stem)):
            raise ValueError('Payload path contains a reserved/nonportable component')
    return path


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate manifest key')
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError('Non-finite JSON values are not accepted')


def create(args):
    # Optional authoring-only dependency; the application uses pinned Monocypher.
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

    payload = args.payload.resolve(strict=True)
    manifest_path = args.manifest.resolve(strict=True)
    key_path = args.private_key.resolve(strict=True)
    output = args.output.resolve()
    if not payload.is_dir() or output.exists() or output.is_relative_to(payload):
        raise ValueError('Choose a new output folder outside the payload directory')
    if key_path.is_relative_to(payload) or key_path.is_relative_to(output):
        raise ValueError('The signing key must be outside payload and output folders')
    raw = manifest_path.read_bytes()
    if len(raw) > 128 * 1024:
        raise ValueError('Manifest exceeds 128 KiB')
    plugin = json.loads(raw.decode('utf-8-sig'), object_pairs_hook=no_duplicates, parse_constant=reject_constant)
    if (not isinstance(plugin, dict) or not re.fullmatch(r'[a-z][a-z0-9._-]{0,99}', plugin.get('id', ''))
            or not re.fullmatch(r'\d+\.\d+\.\d+', plugin.get('version', ''))):
        raise ValueError('Manifest requires a portable lowercase ID and numeric version')
    portable(plugin['id'])
    portable(plugin['library'])
    if '/' in plugin['library'] or type(plugin.get('abi')) is not int or plugin['abi'] not in (1, 2):
        raise ValueError('Manifest requires a native library basename and ABI 1 or 2')
    if not 1 <= len(args.files) <= 256:
        raise ValueError('Select 1 to 256 explicit payload files with --file')
    suffix = '.dll' if args.platform.startswith('windows-') else '.dylib' if args.platform.startswith('macos-') else '.so'
    libraries = {plugin['library'] + suffix}
    if args.platform.startswith('macos-'):
        libraries.add(plugin['library'] + '.so')
    if not libraries.intersection(args.files):
        raise ValueError('Explicit files must include the declared platform library')
    if not set(args.executable).issubset(args.files):
        raise ValueError('--executable must name an explicitly selected payload file')
    files = []
    seen = {'package.json', 'signature.json', 'manifest.json'}
    for name in args.files:
        relative = portable(name)
        if name.casefold() in seen:
            raise ValueError('Duplicate/reserved payload filename')
        seen.add(name.casefold())
        source = payload
        for part in relative.parts:
            source = source / part
            if source.is_symlink():
                raise ValueError('Payload symbolic links are not accepted')
        source = source.resolve(strict=True)
        if not source.is_file() or not source.is_relative_to(payload) or source in (key_path, manifest_path):
            raise ValueError('Payload must contain explicit regular files inside its source folder')
        if source.stat().st_size > MAX_FILE:
            raise ValueError('Payload file exceeds 64 MiB')
        files.append((name, source))
    for name, _ in files:
        if any(parent.as_posix().casefold() in seen for parent in portable(name).parents if parent.as_posix() != '.'):
            raise ValueError('Payload file/directory collision')
    password = getpass.getpass('Signing key password: ').encode('utf-8') if args.ask_password else None
    key = serialization.load_pem_private_key(key_path.read_bytes(), password=password)
    if not isinstance(key, Ed25519PrivateKey):
        raise ValueError('Signing key must be an existing Ed25519 PKCS8 PEM key')
    public = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    output.mkdir(parents=True, exist_ok=False)
    records = []
    total = 0
    for name, source in sorted(files):
        destination = output.joinpath(*portable(name).parts)
        destination.parent.mkdir(parents=True, exist_ok=True)
        digest = hashlib.sha512()
        count = 0
        with source.open('rb') as incoming, destination.open('xb') as outgoing:
            while block := incoming.read(65536):
                count += len(block)
                total += len(block)
                if count > MAX_FILE or total > MAX_TOTAL:
                    raise ValueError('Payload grew beyond its file/total limit')
                digest.update(block)
                outgoing.write(block)
        records.append({'path': name, 'size': count, 'sha512': digest.hexdigest(),
                        'executable': name in args.executable or bool(source.stat().st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))})
    document = {'schemaVersion': 1, 'platform': args.platform, 'plugin': plugin, 'files': records}
    data = (json.dumps(document, ensure_ascii=False, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('utf-8')
    if len(data) > 256 * 1024:
        raise ValueError('Package metadata exceeds 256 KiB')
    signature = {'algorithm': 'Ed25519', 'publicKey': public.hex(), 'signature': key.sign(DOMAIN + data).hex()}
    (output / 'package.json').write_bytes(data)
    (output / 'signature.json').write_text(json.dumps(signature, indent=2) + '\n', 'utf-8')
    return {'id': plugin['id'], 'version': plugin['version'], 'platform': args.platform,
            'files': len(records), 'bytes': total, 'publisherFingerprint': hashlib.sha512(public).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--file', action='append', dest='files', required=True, help='Explicit portable relative file path; repeat per payload file')
    parser.add_argument('--executable', action='append', default=[], help='Payload file needing POSIX execute permission; repeat if needed')
    parser.add_argument('--platform', required=True, choices=['windows-x64', 'windows-arm64', 'windows-x86', 'linux-x64', 'linux-arm64', 'linux-x86', 'macos-x64', 'macos-arm64'])
    parser.add_argument('--private-key', type=Path, required=True, help='Existing Ed25519 PKCS8 PEM; never included in output')
    parser.add_argument('--ask-password', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(create(args), indent=2))


if __name__ == '__main__':
    main()

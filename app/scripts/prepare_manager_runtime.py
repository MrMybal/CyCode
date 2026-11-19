"""Prepare pinned stdlib-only Manager Python; never run the downloaded interpreter."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import urllib.parse
import urllib.request
from manager_bundle import assemble, load_bundle, lock_digest
ROOT = Path(__file__).resolve().parents[2]

def download(row,cache):
    url=row['url'];parts=urllib.parse.urlsplit(url)
    if (parts.scheme!='https' or parts.hostname not in {'www.python.org','files.pythonhosted.org'} or
        parts.username or parts.password or parts.port not in {None,443} or parts.query or parts.fragment):
        raise ValueError('Unexpected runtime download origin')
    name=row.get('file') or parts.path.rsplit('/',1)[-1]
    if not name or Path(name).name!=name or '/' in name or '\\' in name:
        raise ValueError('Invalid runtime cache filename')
    target=cache/name
    if target.exists():
        if target.is_symlink() or not target.is_file() or target.stat().st_size>64*1024*1024:
            raise ValueError('Invalid runtime cache entry')
        data=target.read_bytes()
        if hashlib.sha256(data).hexdigest()!=row['sha256'] or ('size' in row and len(data)!=row['size']):
            raise ValueError('Changed runtime cache entry retained for inspection: '+name)
        return data
    request=urllib.request.Request(url,headers={'User-Agent':'CyCode-build/mcp-runtime'})
    with urllib.request.urlopen(request,timeout=60) as response:data=response.read(64*1024*1024+1)
    if (len(data)>64*1024*1024 or hashlib.sha256(data).hexdigest()!=row['sha256'] or
        ('size' in row and len(data)!=row['size'])):
        raise ValueError('Runtime archive integrity failed: '+name)
    # Never replace an existing cache file or retry a removed executable.
    with target.open('xb') as stream:stream.write(data)
    return data

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=ROOT/'runtime/downloads/manager-runtime')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    lock = json.loads((ROOT/'app/manager-runtime.lock.json').read_text('utf-8'))
    runtime = (ROOT/'runtime').resolve(); runtime.mkdir(parents=True, exist_ok=True)
    destination = (args.output or runtime/'manager-bundles'/lock_digest(lock)).absolute()
    if not destination.resolve().is_relative_to(runtime) or destination.resolve() == runtime:
        parser.error('Keep prepared bundles inside this checkout runtime directory')
    if destination.exists():
        entries = load_bundle(destination, lock)
    else:
        args.cache.mkdir(parents=True, exist_ok=True)
        entries = assemble(lock, download(lock['python'], args.cache))
        destination.parent.mkdir(parents=True, exist_ok=True)
        staging = Path(tempfile.mkdtemp(prefix='.manager-prepare-', dir=destination.parent)).absolute()
        for name, data in entries.items():
            target = staging/name; target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('xb') as stream: stream.write(data)
        load_bundle(staging, lock)
        if staging.resolve() != staging or destination.exists() or destination.parent.resolve() != staging.parent.resolve():
            raise ValueError('Prepared bundle destination changed; staging preserved')
        staging.rename(destination)
    print(json.dumps({'bundle': str(destination), 'files': len(entries), 'verified': True, 'executed': False}))

if __name__ == '__main__': main()

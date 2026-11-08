"""Prepare a pinned redistributable MCP runtime; never run its interpreter or SDK."""
import argparse
import hashlib
import importlib.metadata
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import urllib.parse
import urllib.request
from mcp_bundle import assemble,load_bundle,lock_digest

ROOT=Path(__file__).resolve().parents[2]

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

def source_wheel(source,lock,cache):
    source=Path(source).resolve(strict=True)
    revision=lock['sdk']['revision']
    observed=subprocess.check_output(['git','-C',str(source),'rev-parse',revision+'^{commit}'],timeout=20).decode().strip()
    if observed!=revision:raise ValueError('Pinned SDK source commit is unavailable')
    for name,version in lock['authoring'].items():
        if importlib.metadata.version(name)!=version:
            raise ValueError('SDK authoring requires '+name+'=='+version+' on the build host')
    # Export the commit, never execute a possibly modified checkout or copy its venv.
    archive=subprocess.check_output(['git','-C',str(source),'archive','--format=tar',revision],timeout=30)
    if len(archive)>32*1024*1024:raise ValueError('SDK source archive exceeds its bound')
    scratch=Path(tempfile.mkdtemp(prefix='sdk-source-',dir=cache)).resolve()
    tree=scratch/'source';tree.mkdir();total=0
    with tarfile.open(fileobj=io.BytesIO(archive)) as package:
        for item in package.getmembers():
            if (not item.name or item.name.startswith('/') or '\\' in item.name or ':' in item.name or
                any(p in {'','.','..'} for p in item.name.rstrip('/').split('/')) or
                not (item.isdir() or item.isfile())):
                raise ValueError('Unsafe SDK source archive member')
            target=tree/item.name
            if item.isdir():target.mkdir(parents=True,exist_ok=True);continue
            total+=item.size
            if item.size>4*1024*1024 or total>32*1024*1024:raise ValueError('Oversized SDK source archive')
            target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as out,package.extractfile(item) as inp:out.write(inp.read())
    wheels=scratch/'wheels';wheels.mkdir()
    process=subprocess.run([sys.executable,'-m','pip','--isolated','wheel','--no-deps','--no-build-isolation',
        '--wheel-dir',str(wheels),str(tree),'--disable-pip-version-check','--no-input'],
        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
    if process.returncode:
        raise RuntimeError('Pinned SDK wheel build failed: '+process.stdout[-4096:].decode('utf-8',errors='replace'))
    candidates=list(wheels.glob('cytools_core-'+lock['sdk']['version']+'-py3-none-any.whl'))
    if len(candidates)!=1:raise ValueError('SDK build did not produce the expected wheel')
    return candidates[0].read_bytes()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core-wheel',type=Path,help='Official CyToolsCore wheel matching the pinned source/backend')
    parser.add_argument('--core-source',type=Path,help='Git repository containing the exact SDK commit')
    parser.add_argument('--cache',type=Path,default=ROOT/'runtime/downloads/mcp-runtime')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.core_wheel and args.core_source:parser.error('Choose the SDK wheel or its pinned Git source')
    lock=json.loads((ROOT/'app/mcp-runtime.lock.json').read_text('utf-8'))
    runtime=(ROOT/'runtime').resolve();runtime.mkdir(parents=True,exist_ok=True)
    destination=(args.output or runtime/'mcp-bundles'/lock_digest(lock)).absolute()
    if not destination.resolve().is_relative_to(runtime) or destination.resolve()==runtime:
        parser.error('The prepared bundle must stay inside this checkout runtime directory')
    if destination.exists():
        entries=load_bundle(destination,lock)
        print(json.dumps({'bundle':str(destination),'files':len(entries),'verified':True,'executed':False}));return
    cache=args.cache.absolute();cache.mkdir(parents=True,exist_ok=True)
    if cache.is_symlink():raise ValueError('Runtime authoring cache must not be redirected')
    python=download(lock['python'],cache)
    wheels={}
    for row in lock['wheels']:
        if row['project'].lower()=='cytools-core':
            if args.core_wheel:
                wheel=args.core_wheel.resolve(strict=True)
                if not wheel.is_file() or wheel.stat().st_size>16*1024*1024:
                    raise ValueError('Invalid SDK wheel')
                data=wheel.read_bytes()
            else:
                source=args.core_source or ROOT/'runtime/deps/cytools-core'
                if not source.is_dir():parser.error('Provide --core-wheel or --core-source for the pinned CyToolsCore SDK')
                data=source_wheel(source,lock,cache)
            wheels[row['file']]=data
        else:wheels[row['file']]=download(row,cache)
    entries=assemble(lock,python,wheels)
    destination.parent.mkdir(parents=True,exist_ok=True)
    staging=Path(tempfile.mkdtemp(prefix='.mcp-prepare-',dir=destination.parent)).absolute()
    for name,data in entries.items():
        target=staging/name;target.parent.mkdir(parents=True,exist_ok=True)
        with target.open('xb') as stream:stream.write(data)
    load_bundle(staging,lock)
    # Both resolved paths are verified before the owned directory rename.
    if (staging.resolve()!=staging or destination.parent.resolve()!=staging.parent.resolve() or
        not staging.resolve().is_relative_to(runtime) or destination.exists() or destination.is_symlink()):
        raise ValueError('Prepared runtime destination changed; preserve the staging evidence')
    staging.rename(destination)
    print(json.dumps({'bundle':str(destination),'files':len(entries),'bytes':sum(map(len,entries.values())),
        'verified':True,'executed':False},indent=2))

if __name__=='__main__':main()

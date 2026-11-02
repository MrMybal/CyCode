"""Install checksum-pinned Rust/JDT LS under runtime only. No global installers."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import platform
import shutil
import sys
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[2]

def archive(item):
    cache=ROOT/'runtime/tools/downloads'
    cache.mkdir(parents=True,exist_ok=True)
    path=cache/(item['sha256']+('.tar.xz' if item['url'].endswith('.xz') else '.tar.gz'))
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:
        print('Downloading '+item['url'],flush=True)
        request=urllib.request.Request(item['url'],headers={'User-Agent':'CyCode-build/0.1'})
        with urllib.request.urlopen(request,timeout=120) as source, path.open('wb') as output:
            shutil.copyfileobj(source,output)
        if hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']: raise RuntimeError('Language tool checksum mismatch')
    return path

def extract(item,destination,strip=0):
    destination.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive(item)) as package:
        for entry in package.getmembers():
            parts=PurePosixPath(entry.name).parts
            if '\\' in entry.name or '..' in parts or PurePosixPath(entry.name).is_absolute(): raise RuntimeError('Unsafe language tool path')
            if len(parts)<=strip: continue
            entry.name=str(PurePosixPath(*parts[strip:]))
            if not (destination/entry.name).resolve().is_relative_to(destination.resolve()): raise RuntimeError('Language tool path escapes runtime')
            package.extract(entry,destination,filter='data')

def ensure_languages(rust=True,java=True):
    lock=json.loads((ROOT/'app/language-tools.lock.json').read_text('utf-8'))
    arch='arm64' if platform.machine().lower() in ('arm64','aarch64') else 'x64'
    key=f'{sys.platform}-{arch}'
    destination=ROOT/'runtime/tools'
    if rust:
        items=lock['rust']['platforms'].get(key)
        if not items: print(f'No pinned Rust toolchain for {key}; configure an installed rust-analyzer.',flush=True)
        else:
            target=destination/'rust'
            stamp=target/'installed.json'
            binary=target/'bin'/('rust-analyzer.exe' if sys.platform=='win32' else 'rust-analyzer')
            expected={name:item['sha256'] for name,item in items.items()}
            old=json.loads(stamp.read_text()) if stamp.exists() else {}
            if old.get('archives')!=expected or not binary.is_file() or old.get('binarySha256')!=hashlib.sha256(binary.read_bytes()).hexdigest():
                for name,item in items.items():
                    # Official Rust component archives are root/component/files.
                    # Manifests and install scripts are not executed.
                    component=destination/'rust-components'/name
                    extract(item,component,1)
                    for directory in component.iterdir():
                        if directory.is_dir(): shutil.copytree(directory,target,dirs_exist_ok=True)
                if not binary.is_file(): raise RuntimeError('Rust analyzer missing from component archive')
                stamp.write_text(json.dumps({'version':lock['rust']['version'],'archives':expected,'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n')
                print('Installed private Rust toolchain and rust-analyzer',flush=True)
    if java:
        item=lock['jdtls'];target=destination/'jdtls';stamp=target/'installed.json'
        old=json.loads(stamp.read_text()) if stamp.exists() else {}
        launchers=list((target/'plugins').glob('org.eclipse.equinox.launcher_*.jar'))
        if old.get('sha256')!=item['sha256'] or not launchers or old.get('launcherSha256')!=hashlib.sha256(launchers[0].read_bytes()).hexdigest():
            extract(item,target)
            launchers=list((target/'plugins').glob('org.eclipse.equinox.launcher_*.jar'))
            if not launchers: raise RuntimeError('JDT LS launcher missing from archive')
            stamp.write_text(json.dumps({'version':item['version'],'sha256':item['sha256'],'launcherSha256':hashlib.sha256(launchers[0].read_bytes()).hexdigest()},indent=2)+'\n')
            print('Installed Eclipse JDT LS (requires installed Java 21+)',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--rust',action='store_true');parser.add_argument('--java',action='store_true');parser.add_argument('--web',action='store_true')
    args=parser.parse_args();all_=not any((args.rust,args.java,args.web))
    ensure_languages(rust=all_ or args.rust,java=all_ or args.java)
    if all_ or args.web:
        from setup_web import ensure_web
        ensure_web()

"""Prepare an owned Windows CI renderer; never install a system/product driver."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import urllib.request

ROOT=Path(__file__).resolve().parents[2]

def sha(data):return hashlib.sha256(data).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    args=parser.parse_args()
    if sys.platform!='win32' or os.environ.get('GITHUB_ACTIONS')!='true':
        parser.error('Software renderer preparation is restricted to an explicit Windows GitHub Actions job.')
    executable=args.executable.resolve(strict=True)
    if executable!=ROOT/'runtime/build-vs/Release/CyCode.exe' or not executable.is_file():
        parser.error('Use this checkout production executable; installed/user applications are never changed.')
    archive_tool=shutil.which('7z')
    if not archive_tool:raise RuntimeError('The runner needs 7-Zip for this pinned test-only archive.')
    lock=json.loads((ROOT/'.github/ci/mesa-windows.lock.json').read_text('utf-8'))
    cache=ROOT/'runtime/downloads/graphics-ci';cache.mkdir(parents=True,exist_ok=True)
    archive=cache/('mesa3d-'+lock['version']+'-release-msvc.7z')
    if archive.exists():
        if archive.is_symlink() or not archive.is_file() or archive.stat().st_size!=lock['size']:
            raise ValueError('Changed renderer cache retained for inspection.')
        data=archive.read_bytes()
    else:
        with urllib.request.urlopen(lock['url'],timeout=60) as response:data=response.read(lock['size']+1)
        if len(data)!=lock['size'] or sha(data)!=lock['sha256']:
            raise ValueError('Software renderer archive integrity failed.')
        with archive.open('xb') as stream:stream.write(data)
    if len(data)!=lock['size'] or sha(data)!=lock['sha256']:
        raise ValueError('Changed renderer cache retained for inspection; no automatic download retry.')
    parent=ROOT/'runtime/test-tools';parent.mkdir(parents=True,exist_ok=True)
    fixture=Path(tempfile.mkdtemp(prefix='windows-graphics-',dir=parent)).resolve()
    if fixture.parent!=parent.resolve():raise ValueError('Unexpected renderer fixture directory.')
    expected={'x64/opengl32.dll','x64/libgallium_wgl.dll','x64/dxil.dll'}
    if {entry['member'] for entry in lock['files']}!=expected or len(lock['files'])!=3:
        raise ValueError('Unexpected test-only renderer file set.')
    for entry in lock['files']:
        # Exact member selection streams bytes; no bundled installer/batch script
        # is extracted or executed, and no source/production folder is changed.
        result=subprocess.run([archive_tool,'x','-so',str(archive),entry['member']],
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True,timeout=90)
        binary=result.stdout
        if len(binary)!=entry['size'] or sha(binary)!=entry['sha256'] or binary[:2]!=b'MZ':
            raise ValueError('Software renderer member integrity failed.')
        offset=struct.unpack_from('<I',binary,60)[0]
        if binary[offset:offset+4]!=b'PE\0\0' or struct.unpack_from('<H',binary,offset+4)[0]!=0x8664:
            raise ValueError('Software renderer is not the pinned Windows x64 image.')
        with (fixture/Path(entry['member']).name).open('xb') as stream:stream.write(binary)
    target=fixture/'CyCode.exe'
    original=executable.read_bytes()
    with target.open('xb') as stream:stream.write(original)
    assert sha(target.read_bytes())==sha(original)
    record={'project':lock['project'],'version':lock['version'],'archiveSha256':lock['sha256'],
        'files':lock['files'],'executableSha256':sha(original),'systemOrProductDriverChanged':False}
    (fixture/'renderer.json').write_text(json.dumps(record,indent=2)+'\n','utf-8')
    output=os.environ.get('GITHUB_OUTPUT')
    if output:
        with Path(output).open('a',encoding='utf-8') as stream:stream.write('executable='+str(target)+'\n')
    print('Prepared pinned llvmpipe in an owned runner-only fixture. No driver or application was executed by preparation.')

if __name__=='__main__':main()

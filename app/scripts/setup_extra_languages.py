"""Optional pinned Python/GLSL servers; private runtime, no lifecycle scripts."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import platform
import shutil
import sys
import zipfile
from setup_languages import archive, extract, ROOT

def ensure_extra(python=True,glsl=True):
    lock=json.loads((ROOT/'app/language-tools.lock.json').read_text('utf-8'))
    if python:
        if not shutil.which('node'): raise RuntimeError('Install Node.js to run the optional Pyright server.')
        item=lock['pyright'];destination=ROOT/'runtime/tools/python-languages/node_modules/pyright'
        stamp=destination/'installed.json';entry=destination/'langserver.index.js'
        old=json.loads(stamp.read_text('utf-8')) if stamp.is_file() else {}
        if old.get('archiveSha256')!=item['sha256'] or not entry.is_file() or old.get('entrySha256')!=hashlib.sha256(entry.read_bytes()).hexdigest():
            extract(item,destination,1)
            if not entry.is_file():raise RuntimeError('Pyright language-server entry missing')
            stamp.write_text(json.dumps({'version':item['version'],'archiveSha256':item['sha256'],'entrySha256':hashlib.sha256(entry.read_bytes()).hexdigest()},indent=2)+'\n','utf-8')
        print('Private Pyright '+item['version']+' ready',flush=True)
    if glsl:
        arch='arm64' if platform.machine().lower() in ('arm64','aarch64') else 'x64'
        key=sys.platform+'-'+arch;item=lock['glsl_analyzer']['platforms'].get(key)
        if not item:
            print('No pinned GLSL server for '+key+'; configure an installed server.',flush=True);return
        destination=ROOT/'runtime/tools/glsl-analyzer';destination.mkdir(parents=True,exist_ok=True)
        name='glsl_analyzer.exe' if sys.platform=='win32' else 'glsl_analyzer'
        entry=destination/'bin'/name;stamp=destination/'installed.json'
        old=json.loads(stamp.read_text('utf-8')) if stamp.is_file() else {}
        if old.get('archiveSha256')!=item['sha256'] or not entry.is_file() or old.get('entrySha256')!=hashlib.sha256(entry.read_bytes()).hexdigest():
            with zipfile.ZipFile(archive(item)) as package:
                members=package.infolist()
                if len(members)>256 or sum(m.file_size for m in members)>64*1024*1024:raise RuntimeError('GLSL archive exceeds limits')
                for member in members:
                    path=PurePosixPath(member.filename)
                    if path.is_absolute() or '..' in path.parts or '\\' in member.filename:raise RuntimeError('Unsafe GLSL archive path')
                    if path==PurePosixPath('bin')/name:
                        entry.parent.mkdir(parents=True,exist_ok=True);entry.write_bytes(package.read(member))
            if not entry.is_file():raise RuntimeError('GLSL analyzer binary missing')
            if sys.platform!='win32':entry.chmod(0o755)
            stamp.write_text(json.dumps({'version':lock['glsl_analyzer']['version'],'archiveSha256':item['sha256'],'entrySha256':hashlib.sha256(entry.read_bytes()).hexdigest()},indent=2)+'\n','utf-8')
        shutil.copy2(ROOT/'app/licenses/GLSL-Analyzer.txt',destination/'LICENSE.txt')
        print('Private GLSL Analyzer '+lock['glsl_analyzer']['version']+' ready',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--python',action='store_true');p.add_argument('--glsl',action='store_true');a=p.parse_args()
    both=not a.python and not a.glsl;ensure_extra(python=both or a.python,glsl=both or a.glsl)

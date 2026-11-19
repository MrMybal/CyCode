"""Reproducible stdlib-only Manager Python authoring; never execute a payload."""
import hashlib
import io
import json
from pathlib import Path
import re
import stat
import zipfile
from release_package import package_path

MAX_BYTES=96*1024*1024
MAX_FILES=3000
PREFIX='tools/python/'
RECORD='cycode-runtime.json'

def digest(data):return hashlib.sha256(data).hexdigest()
def lock_digest(lock):return digest(json.dumps(lock,sort_keys=True,separators=(',',':')).encode('utf-8'))

def relative(name):
    return (isinstance(name,str) and name and not name.startswith('/') and '\\' not in name and
            all(p not in {'','.','..'} for p in name.split('/')) and package_path(PREFIX+name))

def members(data):
    if not isinstance(data,bytes) or len(data)>64*1024*1024:raise ValueError('Runtime archive exceeds its bound')
    result={};total=0
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for item in archive.infolist():
            # ZipInfo normalizes Windows separators and truncates NUL in
            # filename. Validate the original central-directory name first.
            name=item.orig_filename.rstrip('/') if item.is_dir() else item.orig_filename
            if (not name or any(ord(c)<32 for c in name) or name.startswith('/') or '\\' in name or ':' in name or
                any(p in {'','.','..'} for p in name.split('/')) or
                stat.S_IFMT(item.external_attr>>16)==stat.S_IFLNK):
                raise ValueError('Unsafe runtime archive member')
            if item.is_dir():continue
            total+=item.file_size
            if name in result or len(result)>=MAX_FILES or item.file_size>64*1024*1024 or total>MAX_BYTES:
                raise ValueError('Duplicate or oversized runtime archive')
            result[name]=archive.read(item)
    return result

def payload_files(lock,python_archive,wheels=None):
    if digest(python_archive)!=lock['python']['sha256']:raise ValueError('Python archive SHA-256 mismatch')
    if wheels: raise ValueError('Manager runtime contains stdlib only; no SDK or third-party wheels')
    entries={};excluded=[]
    def add(name,data):
        if (not relative(name) or name.lower() in {n.lower() for n in entries} or
            len(entries)>=MAX_FILES or not isinstance(data,bytes) or len(data)>64*1024*1024):
            raise ValueError('Unsafe or duplicate installed runtime path: '+name)
        entries[name]=data
    for name,data in members(python_archive).items():add(name,data)
    if not {'python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt'}<=entries.keys():
        raise ValueError('Incomplete embedded Python distribution')
    entries['python313._pth']=b'python313.zip\n.\n../../app\n'
    if sum(map(len,entries.values()))>MAX_BYTES:raise ValueError('Installed Manager runtime exceeds its byte bound')
    for name in entries:
        if any(name[:i] in entries for i,c in enumerate(name) if c=='/'):
            raise ValueError('Runtime file/directory collision')
    return entries,excluded

def inventory(entries):
    return [{'path':n,'size':len(b),'sha256':digest(b)} for n,b in sorted(entries.items())]

def payload_digest(rows):
    return digest(json.dumps(rows,sort_keys=True,separators=(',',':')).encode('utf-8'))

def assemble(lock,python_archive,wheels=None):
    entries,excluded=payload_files(lock,python_archive,wheels)
    rows=inventory(entries)
    if payload_digest(rows)!=lock['payloadSha256']:
        raise ValueError('Assembled runtime differs from the reviewed pinned payload')
    record={'format':1,'lockSha256':lock_digest(lock),'python':lock['python']['version'],
        'purpose':'manager-stdlib',
        'excluded':excluded,'files':rows}
    entries[RECORD]=(json.dumps(record,indent=2)+'\n').encode('utf-8')
    return {PREFIX+name:data for name,data in entries.items()}

def ordinary(path):
    status=path.lstat()
    if path.is_symlink() or getattr(status,'st_file_attributes',0)&1024:
        raise ValueError('Redirected runtime bundle path')

def load_bundle(directory,lock):
    directory=Path(directory).absolute();ordinary(directory)
    base=directory/'tools/python';ordinary(directory/'tools');ordinary(base)
    marker=base/RECORD;ordinary(marker)
    if marker.stat().st_size>2*1024*1024:raise ValueError('Runtime inventory exceeds its bound')
    record=json.loads(marker.read_text('utf-8'))
    if (record.get('format')!=1 or record.get('lockSha256')!=lock_digest(lock) or
        record.get('python')!=lock['python']['version'] or
        record.get('purpose')!='manager-stdlib'):
        raise ValueError('Runtime bundle differs from the selected lock')
    rows=record['files']
    if not isinstance(rows,list) or not rows or len(rows)>MAX_FILES:raise ValueError('Invalid runtime inventory')
    if payload_digest(rows)!=lock['payloadSha256']:raise ValueError('Runtime inventory differs from pinned payload')
    entries={};total=0;seen=set()
    for row in rows:
        name=row['path'];size=row['size'];sha=row['sha256']
        if (not relative(name) or name.lower() in seen or name==RECORD or type(size) is not int or
            not 0<=size<=64*1024*1024 or not isinstance(sha,str) or not re.fullmatch('[0-9a-f]{64}',sha)):
            raise ValueError('Invalid runtime inventory entry')
        seen.add(name.lower());total+=size
        if total>MAX_BYTES:raise ValueError('Runtime inventory exceeds its byte bound')
        path=base/name
        for parent in [path,*path.parents]:
            ordinary(parent)
            if parent==base:break
        if not path.is_file() or path.stat().st_size!=size:raise ValueError('Runtime payload size changed')
        data=path.read_bytes()
        if digest(data)!=sha:raise ValueError('Runtime payload hash changed')
        entries[PREFIX+name]=data
    expected={base/name for name in [*[r['path'] for r in rows],RECORD]}
    for item in base.rglob('*'):
        ordinary(item)
        if item.is_file() and item not in expected:raise ValueError('Unexpected file in the runtime bundle')
    entries[PREFIX+RECORD]=marker.read_bytes()
    required={'python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt'}
    if not {PREFIX+n for n in required}<=entries.keys():raise ValueError('Incomplete Manager runtime bundle')
    if entries[PREFIX+'python313._pth']!=b'python313.zip\n.\n../../app\n':
        raise ValueError('Runtime import isolation changed')
    return entries

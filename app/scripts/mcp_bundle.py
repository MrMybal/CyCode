"""Reproducible wheel/embedded-Python authoring; never import or execute a payload."""
import base64
import csv
import email.parser
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import posixpath
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

def sdk_files(files,lock):
    expected=lock['sdk']['normalizedPackageFilesSha256']
    actual={n:digest(b.replace(b'\r\n',b'\n')) for n,b in files.items() if n.startswith(('cytools_core/','cytool_test/'))}
    if actual!=expected:raise ValueError('SDK package differs from the pinned published source')

def payload_files(lock,python_archive,wheels):
    if digest(python_archive)!=lock['python']['sha256']:raise ValueError('Python archive SHA-256 mismatch')
    if set(wheels)!={r['file'] for r in lock['wheels']}:raise ValueError('Incomplete runtime wheel set')
    entries={};excluded=[]
    def add(name,data):
        if (not relative(name) or name.lower() in {n.lower() for n in entries} or
            len(entries)>=MAX_FILES or not isinstance(data,bytes) or len(data)>64*1024*1024):
            raise ValueError('Unsafe or duplicate installed runtime path: '+name)
        entries[name]=data
    for name,data in members(python_archive).items():add(name,data)
    if not {'python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt'}<=entries.keys():
        raise ValueError('Incomplete embedded Python distribution')
    for row in lock['wheels']:
        data=wheels[row['file']]
        is_sdk=row['project'].lower()=='cytools-core'
        if not is_sdk and (digest(data)!=row['sha256'] or len(data)!=row['size']):
            raise ValueError('Dependency wheel SHA-256/size mismatch: '+row['file'])
        files=members(data)
        info=[n[:-len('/METADATA')] for n in files if n.endswith('.dist-info/METADATA')]
        if len(info)!=1:raise ValueError('Invalid wheel metadata directory')
        info=info[0];meta=email.parser.BytesParser().parsebytes(files[info+'/METADATA'])
        canonical=lambda s:re.sub('[-_.]+','-',s).lower()
        if canonical(meta['Name'])!=canonical(row['project']) or meta['Version']!=row['version']:
            raise ValueError('Wheel identity mismatch')
        if is_sdk:
            sdk_files(files,lock)
            # A pure-Python SDK is rebuilt from its fixed published source with
            # the same pinned backend. Normalize source checkout line endings.
            files={n:b.replace(b'\r\n',b'\n') for n,b in files.items()}
            expected_generator='Generator: setuptools ('+lock['authoring']['setuptools']+')'
            if expected_generator not in files[info+'/WHEEL'].decode('utf-8'):
                raise ValueError('SDK wheel was built with a different backend')
        installed={};data_prefix=info[:-len('.dist-info')]+'.data/'
        for name,content in files.items():
            if name==info+'/RECORD':continue
            if name.startswith(data_prefix):
                scheme,separator,tail=name[len(data_prefix):].partition('/')
                if not separator:raise ValueError('Malformed wheel installation scheme')
                if is_sdk:
                    if scheme=='data' and tail.startswith('share/cytools-core/Docs/') and tail.endswith('.md'):
                        excluded.append({'wheel':row['file'],'path':name,'reason':'upstream-sdk-manual'});continue
                    raise ValueError('Unexpected SDK wheel installation scheme')
                if scheme in {'purelib','platlib'}:target='Lib/site-packages/'+tail
                elif scheme=='data':target=tail
                elif scheme=='scripts':
                    excluded.append({'wheel':row['file'],'path':name,'reason':'dependency-cli'});continue
                else:raise ValueError('Unsupported wheel installation scheme: '+scheme)
            else:
                if is_sdk and not name.startswith(('cytools_core/','cytool_test/',info+'/')):
                    raise ValueError('Unexpected executable SDK package member')
                if canonical(row['project'])=='pywin32' and (name.startswith('pythonwin/') or name=='win32/pythonservice.exe'):
                    excluded.append({'wheel':row['file'],'path':name,'reason':'dependency-gui-service'});continue
                target='Lib/site-packages/'+name
            add(target,content);installed[target]=content
        installer='Lib/site-packages/'+info+'/INSTALLER'
        if installer in entries:raise ValueError('Wheel contains a preinstalled INSTALLER')
        add(installer,b'CyCode\n');installed[installer]=b'CyCode\n'
        # Wheel .data paths are relocated according to PEP 427. Record the actual
        # installed paths/hashes; no absolute build path or generated CLI launcher.
        output=io.StringIO(newline='');writer=csv.writer(output,lineterminator='\n')
        for target,content in sorted(installed.items()):
            encoded=base64.urlsafe_b64encode(hashlib.sha256(content).digest()).decode().rstrip('=')
            writer.writerow([posixpath.relpath(target,'Lib/site-packages'),'sha256='+encoded,len(content)])
        writer.writerow([info+'/RECORD','',''])
        add('Lib/site-packages/'+info+'/RECORD',output.getvalue().encode('utf-8'))
    entries['python313._pth']=b'python313.zip\n.\nLib/site-packages\n../../app\nimport site\n'
    if sum(map(len,entries.values()))>MAX_BYTES:raise ValueError('Installed MCP runtime exceeds its byte bound')
    for name in entries:
        if any(name[:i] in entries for i,c in enumerate(name) if c=='/'):
            raise ValueError('Runtime file/directory collision')
    return entries,excluded

def inventory(entries):
    return [{'path':n,'size':len(b),'sha256':digest(b)} for n,b in sorted(entries.items())]

def payload_digest(rows):
    return digest(json.dumps(rows,sort_keys=True,separators=(',',':')).encode('utf-8'))

def assemble(lock,python_archive,wheels):
    entries,excluded=payload_files(lock,python_archive,wheels)
    rows=inventory(entries)
    if payload_digest(rows)!=lock['payloadSha256']:
        raise ValueError('Assembled runtime differs from the reviewed pinned payload')
    record={'format':1,'lockSha256':lock_digest(lock),'python':lock['python']['version'],
        'sdk':{'version':lock['sdk']['version'],'revision':lock['sdk']['revision']},
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
        record.get('sdk')!={'version':lock['sdk']['version'],'revision':lock['sdk']['revision']}):
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
    required={'python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt',
        'Lib/site-packages/cytools_core/__init__.py','Lib/site-packages/mcp/__init__.py'}
    if not {PREFIX+n for n in required}<=entries.keys():raise ValueError('Incomplete MCP runtime bundle')
    if entries[PREFIX+'python313._pth']!=b'python313.zip\n.\nLib/site-packages\n../../app\nimport site\n':
        raise ValueError('Runtime import isolation changed')
    return entries

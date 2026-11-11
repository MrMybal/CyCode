"""Archive/inventory data only: no interpreter payload, SDK import, EXE file or process."""
import copy
import hashlib
import io
import json
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile

APP=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(APP/'scripts'))
import mcp_bundle as bundle
import prepare_mcp_runtime as prepare

def archive(entries):
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,'w') as output:
        with warnings.catch_warnings():
            warnings.simplefilter('ignore',UserWarning)
            for name,data in entries:
                if isinstance(name,str):
                    # Keep deliberately malformed raw ZIP names even on Windows.
                    item=zipfile.ZipInfo();item.filename=name;item.orig_filename=name
                else:item=name
                output.writestr(item,data)
    return stream.getvalue()

class McpRuntimeDataContracts(unittest.TestCase):
    def test_archives_refuse_traversal_aliases_links_and_limits(self):
        for name in ('../escape','/absolute','a/../escape','C:/drive','a\\escape','a//escape','safe.txt'+chr(0)+'hidden'):
            with self.subTest(path=name),self.assertRaises(ValueError):bundle.members(archive([(name,b'data')]))
        with self.assertRaises(ValueError):bundle.members(archive([('a',b'one'),('a',b'two')]))
        link=zipfile.ZipInfo('link');link.create_system=3;link.external_attr=(stat.S_IFLNK|0o777)<<16
        with self.assertRaises(ValueError):bundle.members(archive([(link,b'target')]))
        with patch.object(bundle,'MAX_BYTES',8),self.assertRaises(ValueError):
            bundle.members(archive([('a',b'ninebytes')]))

    def test_sdk_requires_the_whole_pinned_package_and_exact_source(self):
        name='cytools_core/example.py';source=b'data only\n'
        lock={'sdk':{'normalizedPackageFilesSha256':{name:hashlib.sha256(source).hexdigest()}}}
        bundle.sdk_files({name:b'data only\r\n'},lock)
        for files in ({name:b'changed'}, {name:source,'cytools_core/unreviewed.py':b'other'}, {}):
            with self.assertRaises(ValueError):bundle.sdk_files(files,lock)

    def fixture(self):
        python=archive([(n,b'data-only: never executable') for n in
            ('python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt')])
        sources={'cytools_core/__init__.py':b'data only\n'}
        lock={'python':{'version':'3.13.17','sha256':bundle.digest(python)},
            'sdk':{'version':'0.10.1','revision':'a'*40,
                'normalizedPackageFilesSha256':{n:bundle.digest(b) for n,b in sources.items()}},
            'authoring':{'setuptools':'84.0.0','wheel':'0.45.1'},'wheels':[]}
        wheels={}
        definitions=[('cytools-core','cytools_core',[
                ('cytools_core/__init__.py',b'data only\r\n'),
                ('cytools_core-0.10.1.data/data/share/cytools-core/Docs/Guide.md',b'upstream guide')],'0.10.1'),
            ('example','example',[
                ('example/__init__.py',b'data-only'),
                ('example-1.0.0.data/data/share/example/NOTICE.txt',b'original license'),
                ('example-1.0.0.data/scripts/cli.py',b'unneeded CLI')],'1.0.0'),
            ('pywin32','pywin32',[
                ('win32/win32job.pyd',b'data-only'),('pythonwin/Pythonwin.exe',b'data-only'),
                ('win32/pythonservice.exe',b'data-only')],'1.0.0')]
        for project,name,files,version in definitions:
            info=name+'-'+version+'.dist-info'
            files+=[(info+'/METADATA',('Metadata-Version: 2.4\nName: '+project+'\nVersion: '+version+'\n\n').encode()),
                (info+'/WHEEL',b'Wheel-Version: 1.0\nGenerator: setuptools (84.0.0)\n'),
                (info+'/licenses/LICENSE',b'preserved upstream notice'),(info+'/RECORD',b'')]
            data=archive(files);filename=name+'-'+version+'-py3-none-any.whl'
            wheels[filename]=data;lock['wheels'].append({'project':project,'version':version,
                'file':filename,'size':len(data),'sha256':bundle.digest(data)})
        payload,_=bundle.payload_files(lock,python,wheels)
        lock['payloadSha256']=bundle.payload_digest(bundle.inventory(payload))
        return lock,python,wheels

    def test_wheel_layout_notices_record_and_import_isolation(self):
        lock,python,wheels=self.fixture();files=bundle.assemble(lock,python,wheels)
        prefix='tools/python/'
        self.assertEqual(files[prefix+'Lib/site-packages/cytools_core/__init__.py'],b'data only\n')
        self.assertEqual(files[prefix+'share/example/NOTICE.txt'],b'original license')
        self.assertIn(b'../../share/example/NOTICE.txt',files[prefix+'Lib/site-packages/example-1.0.0.dist-info/RECORD'])
        self.assertEqual(files[prefix+'python313._pth'],b'python313.zip\n.\nLib/site-packages\n../../app\nimport site\n')
        self.assertFalse(any('Pythonwin.exe' in n or 'pythonservice.exe' in n or '/scripts/cli.py' in n for n in files))
        self.assertEqual(files[prefix+'Lib/site-packages/example-1.0.0.dist-info/licenses/LICENSE'],b'preserved upstream notice')
        self.assertTrue(all(n.startswith(prefix) for n in files))

    def test_tampered_archives_and_sdk_data_cannot_inject_runtime_code(self):
        lock,python,wheels=self.fixture()
        with self.assertRaises(ValueError):bundle.assemble(lock,python+b'changed',wheels)
        altered=dict(wheels);name=next(n for n in wheels if n.startswith('example-'))
        altered[name]=wheels[name]+b'changed'
        with self.assertRaises(ValueError):bundle.assemble(lock,python,altered)
        sdk=next(n for n in wheels if n.startswith('cytools_core-'))
        entries=list(bundle.members(wheels[sdk]).items())
        altered=dict(wheels);altered[sdk]=archive(entries+[
            ('cytools_core-0.10.1.data/data/sitecustomize.py',b'not allowed')])
        with self.assertRaises(ValueError):bundle.assemble(lock,python,altered)
        lock['payloadSha256']='0'*64
        with self.assertRaises(ValueError):bundle.assemble(lock,python,wheels)

    def test_changed_inventory_cannot_bless_changed_files(self):
        rows=[{'path':'payload.txt','size':8,'sha256':bundle.digest(b'original')}]
        lock={'python':{'version':'3.13.17'},'sdk':{'version':'0.10.1','revision':'a'*40},
            'payloadSha256':bundle.payload_digest(rows)}
        record={'format':1,'lockSha256':bundle.lock_digest(lock),'python':'3.13.17',
            'sdk':dict(lock['sdk']),'files':copy.deepcopy(rows)}
        with tempfile.TemporaryDirectory(prefix='cycode-mcp-data-') as temporary:
            root=Path(temporary).resolve();self.assertEqual(root.parent,Path(tempfile.gettempdir()).resolve())
            base=root/'tools/python';base.mkdir(parents=True)
            (base/'payload.txt').write_bytes(b'changed!')
            record['files'][0]['sha256']=bundle.digest(b'changed!')
            (base/bundle.RECORD).write_text(json.dumps(record),'utf-8')
            with self.assertRaises(ValueError):bundle.load_bundle(root,lock)
            self.assertEqual((base/'payload.txt').read_bytes(),b'changed!')

    def test_changed_cache_is_preserved_and_never_redownloaded_implicitly(self):
        row={'url':'https://www.python.org/verified.zip','file':'verified.zip',
            'sha256':bundle.digest(b'expected')}
        with tempfile.TemporaryDirectory(prefix='cycode-mcp-cache-data-') as temporary:
            root=Path(temporary).resolve();self.assertEqual(root.parent,Path(tempfile.gettempdir()).resolve())
            target=root/'verified.zip';target.write_bytes(b'changed')
            with patch('urllib.request.urlopen') as network,self.assertRaises(ValueError):prepare.download(row,root)
            network.assert_not_called();self.assertEqual(target.read_bytes(),b'changed')
            for url in ('http://www.python.org/a','file:///local','https://u:p@www.python.org/a',
                        'https://www.python.org/a?secret=x','https://example.invalid/a'):
                with patch('urllib.request.urlopen') as network,self.assertRaises(ValueError):
                    prepare.download({**row,'url':url},root)
                network.assert_not_called()

if __name__=='__main__':unittest.main()

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
import manager_bundle as bundle
import prepare_manager_runtime as prepare

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

    def fixture(self):
        python=archive([(n,b'data-only: never executable') for n in
            ('python.exe','python313.dll','python313.zip','python313._pth','LICENSE.txt')])
        lock={'purpose':'manager-stdlib','python':{'version':'3.13.17','sha256':bundle.digest(python)}}
        payload,_=bundle.payload_files(lock,python)
        lock['payloadSha256']=bundle.payload_digest(bundle.inventory(payload))
        return lock,python

    def test_stdlib_only_payload_and_import_isolation(self):
        lock,python=self.fixture();files=bundle.assemble(lock,python)
        prefix='tools/python/'
        self.assertEqual(files[prefix+'python313._pth'],b'python313.zip\n.\n../../app\n')
        self.assertFalse(any('site-packages' in n or n.endswith('.whl') for n in files))
        self.assertIn(prefix+'LICENSE.txt',files)
        self.assertTrue(all(n.startswith(prefix) for n in files))
        with self.assertRaises(ValueError):bundle.assemble(lock,python,{'unwanted.whl':b'wheel'})

    def test_tampered_archive_or_payload_cannot_be_blessed(self):
        lock,python=self.fixture()
        with self.assertRaises(ValueError):bundle.assemble(lock,python+b'changed')
        lock['payloadSha256']='0'*64
        with self.assertRaises(ValueError):bundle.assemble(lock,python)

    def test_changed_inventory_cannot_bless_changed_files(self):
        rows=[{'path':'payload.txt','size':8,'sha256':bundle.digest(b'original')}]
        lock={'python':{'version':'3.13.17'},'payloadSha256':bundle.payload_digest(rows)}
        record={'format':1,'lockSha256':bundle.lock_digest(lock),'python':'3.13.17',
            'purpose':'manager-stdlib','files':copy.deepcopy(rows)}
        with tempfile.TemporaryDirectory(prefix='cycode-manager-data-') as temporary:
            root=Path(temporary).resolve()
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

"""Data-only adapter archive preparation; no executable bytes, network or process."""
import hashlib
import io
import json
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest.mock import patch
import zipfile

APP=Path(__file__).resolve().parents[1]

class DebuggerArchiveModes(unittest.TestCase):
    def test_macos_zip_restores_modes_and_repairs_nonexecutable_cache(self):
        api=runpy.run_path(str(APP/'scripts/setup_debugger.py'))
        ensure=api['ensure_netcoredbg']
        buffer=io.BytesIO()
        with zipfile.ZipFile(buffer,'w') as archive:
            for name,mode in [('netcoredbg/netcoredbg',0o100755),('netcoredbg/notice.txt',0o100644)]:
                item=zipfile.ZipInfo(name);item.create_system=3;item.external_attr=mode<<16
                archive.writestr(item,b'data-only archive fixture; never executable')
        data=buffer.getvalue()
        with tempfile.TemporaryDirectory(prefix='cycode-archive-data-') as temporary:
            root=Path(temporary); (root/'app').mkdir(); (root/'runtime/tools').mkdir(parents=True)
            (root/'runtime/tools/netcoredbg-darwin-arm64.zip').write_bytes(data)
            (root/'app/debugger-tools.lock.json').write_text(json.dumps({'netcoredbg':{
                'version':'fixture','platforms':{'darwin-arm64':{'url':'https://example.invalid/never-requested.zip',
                'sha256':hashlib.sha256(data).hexdigest()}}}}),'utf-8')
            with patch.dict(ensure.__globals__,{'ROOT':root}), patch('sys.platform','darwin'), \
                 patch('platform.machine',return_value='arm64'), patch('urllib.request.urlopen') as network, \
                 patch('subprocess.run') as execute, patch.object(Path,'chmod') as permissions:
                with patch('os.access',return_value=False): ensure()
                binary=root/'runtime/tools/netcoredbg/netcoredbg/netcoredbg'
                self.assertTrue(binary.is_file(),str([(p.relative_to(root).as_posix(),p.is_file()) for p in root.rglob('*')]))
                self.assertEqual(binary.read_bytes(),b'data-only archive fixture; never executable')
                self.assertGreaterEqual(permissions.call_count,1)
                # A complete cached but non-executable installation must be repaired.
                permissions.reset_mock()
                with patch('os.access',return_value=False): ensure()
                self.assertGreaterEqual(permissions.call_count,1)
                permissions.reset_mock()
                with patch('os.access',return_value=True): ensure()
                permissions.assert_not_called()
                network.assert_not_called(); execute.assert_not_called()

if __name__=='__main__': unittest.main()

"""Native source dependency selection. No downloads or executable processes."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('native_fetch',Path(__file__).resolve().parents[1]/'scripts/fetch_dependencies.py')
fetch=importlib.util.module_from_spec(spec);spec.loader.exec_module(fetch)
class Components(unittest.TestCase):
    def test_text_components_fetch_only_json(self):
        data=io.BytesIO()
        with tarfile.open(fileobj=data,mode='w:gz') as archive:
            member=tarfile.TarInfo('json/include/header.hpp');body=b'// fixture';member.size=len(body);archive.addfile(member,io.BytesIO(body))
        payload=data.getvalue();digest=hashlib.sha256(payload).hexdigest()
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'app').mkdir()
            lock={name:{'url':'https://example.invalid/'+name,'revision':'fixture','sha256':digest} for name in ['json','vterm','tinyxml2','monocypher','pcre2','utf8proc']}
            (root/'app/dependencies.lock.json').write_text(json.dumps(lock))
            def download(url,destination):destination.write_bytes(payload)
            with patch.object(fetch,'ROOT',root),patch.object(fetch,'DEPS',root/'deps'),patch.object(fetch,'download',side_effect=download) as network:
                fetch.main(['--components','TextSession','TextBridge','Process'])
            self.assertEqual(network.call_count,1)
            self.assertTrue((root/'deps/json/.cycode-dependency').is_file())
            self.assertFalse((root/'deps/vterm').exists())
            self.assertFalse((root/'deps/monocypher').exists())
    def test_unknown_component_fails_before_fetching(self):
        with patch.object(fetch,'download',side_effect=AssertionError('unexpected network')):
            with self.assertRaises(SystemExit):fetch.main(['--components','Nonexistent'])
if __name__=='__main__':unittest.main()

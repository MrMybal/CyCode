"""Data-only tests: in-memory archives, no EXE files/processes or SDK Runtime."""
import hashlib
import io
import json
from pathlib import Path
import runpy
import struct
import unittest
import zipfile

api = runpy.run_path(str(Path(__file__).resolve().parents[1] / 'scripts/release_package.py'))


class ReleasePackageTests(unittest.TestCase):
    def entries(self):
        return {name: ('data-only fixture: ' + name).encode() for name in api['REQUIRED']}

    def test_stored_update_matches_manifest(self):
        entries = api['payload'](self.entries(), '0.2.1', {'sourceCommit': 'fixture'})
        output = io.BytesIO()
        api['write_archive'](output, entries, '0.2.1', (2026, 1, 1, 0, 0, 0), update=True)
        with zipfile.ZipFile(output) as archive:
            self.assertIsNone(archive.testzip())
            self.assertTrue(all(row.compress_type == zipfile.ZIP_STORED for row in archive.infolist()))
            manifest = json.loads(archive.read('CyCode-0.2.1/manifest.json'))
            self.assertEqual(manifest['updateFormat'], 1)
            self.assertEqual(manifest['platform'], 'windows-x64')
            self.assertEqual(len(manifest['files']) + 1, len(archive.namelist()))
            for item in manifest['files']:
                data = archive.read('CyCode-0.2.1/' + item['path'])
                self.assertEqual(item['size'], len(data))
                self.assertEqual(item['sha256'], hashlib.sha256(data).hexdigest())

    def test_managed_paths_preserve_private_data(self):
        for name in ('../CyCode.exe', 'data/settings/x.json', 'app/data/token.json', '/CyCode.exe',
                     'app/runtime/config.json', 'app/CON.txt', 'app/x.', 'app/x\\y',
                     'app/.git/config', 'app/.CyDocBuild/cydoc.config.json', 'app//x', 'app/AUX', 'tools/lpt1.txt', 'app/c:y'):
            with self.subTest(name=name):
                self.assertFalse(api['package_path'](name))
        self.assertTrue(api['package_path']('app/Docs/Updates.md'))
        self.assertTrue(api['package_path']('app/Documentation/DocData/viewer.js'))

    def test_reject_missing_case_collision_prefix_conflict(self):
        for patch in ({'cycode.exe': b'collision'}, {'app': b'not a package file'},
                      {'app/file.txt': b'file', 'app/file.txt/nested': b'conflict'}):
            with self.assertRaises(ValueError):
                api['payload'](dict(self.entries(), **patch), '0.2.1', {})
        entries = self.entries()
        entries.pop('CyCodeUpdater.exe')
        with self.assertRaises(ValueError):
            api['payload'](entries, '0.2.1', {})

    def test_pe_architecture_without_loading_a_binary(self):
        header = bytearray(64 + 26)
        header[:2] = b'MZ'
        struct.pack_into('<I', header, 60, 64)
        header[64:68] = b'PE\x00\x00'
        for offset, value in ((68, 0x8664), (84, 240), (86, 2), (88, 0x20b)):
            struct.pack_into('<H', header, offset, value)
        self.assertEqual(api['pe_machine'](io.BytesIO(header)), 0x8664)
        for offset, value in ((88, 0x10b), (86, 0x2002), (84, 0)):
            invalid = bytearray(header)
            struct.pack_into('<H', invalid, offset, value)
            with self.assertRaises(ValueError):
                api['pe_machine'](io.BytesIO(invalid))

    def test_versions_and_reproducible_bytes(self):
        for version in ('0.2', 'v0.2.1', '01.2.3', '1.2.3-beta', '1.2.65536', '1.2.3\n'):
            with self.assertRaises(ValueError):
                api['version_tuple'](version)
        entries = api['payload'](self.entries(), '0.2.1', {})
        results = []
        for _ in range(2):
            output = io.BytesIO()
            api['write_archive'](output, entries, '0.2.1', (2026, 1, 1, 0, 0, 0), update=True)
            results.append(output.getvalue())
        self.assertEqual(*results)


if __name__ == '__main__':
    unittest.main()

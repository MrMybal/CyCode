"""Product-bound release authoring: in-memory data only, no native executable."""
import io
import json
from pathlib import Path
import runpy
import unittest
import zipfile

API = runpy.run_path(str(Path(__file__).resolve().parents[1] / 'scripts/release_package.py'))


class UpdateProducts(unittest.TestCase):
    def setUp(self):
        self.ide = API['UpdateProduct']('CyCodeIDE', 'MrMybal/CyCodeIDE')
        self.entries = {name: ('data-only ' + name).encode() for name in API['required_files'](self.ide)}
        self.entries.update({'VERSION.md': b'English notes', 'extensions/p4/provider.json': b'provider data',
                             'tools/python/Lib/site-packages/mcp/__init__.py': b'MCP package data'})

    def test_ide_names_and_manifest_are_consistent(self):
        result = API['payload'](self.entries, '0.2.3', {'sourceCommit': 'reviewed'}, product=self.ide)
        manifest = json.loads(result['manifest.json'])
        self.assertEqual((manifest['product'], manifest['repository']), ('CyCodeIDE', 'MrMybal/CyCodeIDE'))
        self.assertEqual(self.ide.installation_marker, 'CyCodeIDE-installed.flag')
        self.assertEqual(self.ide.installation_marker_bytes, b'CyCodeIDE per-user installation\n')
        stream = io.BytesIO()
        API['write_archive'](stream, result, '0.2.3', (2026, 1, 1, 0, 0, 0), update=True, product=self.ide)
        with zipfile.ZipFile(stream) as archive:
            self.assertEqual(set(archive.namelist()), {'CyCodeIDE-0.2.3/' + name for name in result})
            self.assertTrue(all(row.compress_type == zipfile.ZIP_STORED for row in archive.infolist()))
            self.assertEqual(archive.read('CyCodeIDE-0.2.3/extensions/p4/provider.json'), b'provider data')
            self.assertEqual(archive.read('CyCodeIDE-0.2.3/tools/python/Lib/site-packages/mcp/__init__.py'), b'MCP package data')
            self.assertIsNone(archive.testzip())

    def test_foreign_executables_and_private_paths_refuse(self):
        for name in ('CyCode.exe', 'CyCodeUpdater.exe', 'CyCodeReader.exe', 'CyCodeReaderUpdater.exe',
                     '../CyCodeIDE.exe', 'data/settings.json', 'app/.CyDocBuild/cache.json'):
            self.assertFalse(API['package_path'](name, product=self.ide))
        self.assertTrue(API['package_path']('CyCodeIDE.exe', product=self.ide))
        self.assertFalse(API['package_path']('CyCodeIDE.exe'))
        with self.assertRaises(ValueError):
            API['payload'](dict(self.entries, **{'CyCode.exe': b'foreign'}), '0.2.3', {}, product=self.ide)

    def test_manifest_cannot_be_relabelled_or_changed_after_payload_validation(self):
        prepared = API['payload'](self.entries, '0.2.3', {}, product=self.ide)
        for change in ({'product': 'CyCode'}, {'repository': 'MrMybal/CyCodeReader'}, {'version': '0.2.4'}):
            foreign = dict(prepared)
            manifest = dict(json.loads(prepared['manifest.json']), **change)
            foreign['manifest.json'] = json.dumps(manifest).encode()
            stream = io.BytesIO(b'keep destination')
            with self.assertRaises(ValueError):
                API['write_archive'](stream, foreign, '0.2.3', (2026, 1, 1, 0, 0, 0), update=True, product=self.ide)
            self.assertEqual(stream.getvalue(), b'keep destination')
        with self.assertRaises(ValueError):
            API['payload'](self.entries, '0.2.3', {'repository': 'MrMybal/CyCode'}, product=self.ide)
        changed = dict(prepared, **{'CyCodeIDE.exe': b'changed after validation'})
        with self.assertRaises(ValueError):
            API['write_archive'](io.BytesIO(), changed, '0.2.3', (2026, 1, 1, 0, 0, 0), update=True, product=self.ide)

    def test_product_inputs_and_required_host_resources_are_guarded(self):
        for product in ('../IDE', 'IDE.exe', 'IDE;value', 'NUL', 'LPT1', 'IDE\n', 'éIDE', 'X' * 65):
            with self.assertRaises(ValueError):
                API['UpdateProduct'](product, 'MrMybal/CyCodeIDE')
        for repository in ('https://github.com/MrMybal/CyCodeIDE', '../CyCodeIDE', 'owner/repo/other',
                           'owner/repo\n', 'owner/repo;token'):
            with self.assertRaises(ValueError):
                API['UpdateProduct']('CyCodeIDE', repository)
        with self.assertRaises(ValueError):
            API['payload'](self.entries, '0.2.3', {}, product=self.ide, required={'tools/python/python.exe'})
        for name in ('CyCodeIDE.exe', 'CyCodeIDEUpdater.exe'):
            with self.assertRaises(ValueError):
                API['payload']({key: value for key, value in self.entries.items() if key != name},
                               '0.2.3', {}, product=self.ide)

    def test_legacy_authoring_keeps_its_required_distribution(self):
        legacy = API['DEFAULT_PRODUCT']
        self.assertEqual(legacy.installation_marker, 'installed.flag')
        self.assertEqual(legacy.installation_marker_bytes, b'CyCode per-user installation\n')
        entries = {name: b'legacy fixture' for name in API['REQUIRED']}
        prepared = API['payload'](entries, '0.2.3', {})
        manifest = json.loads(prepared['manifest.json'])
        del manifest['product'], manifest['repository']
        prepared['manifest.json'] = json.dumps(manifest).encode()
        API['write_archive'](io.BytesIO(), prepared, '0.2.3', (2026, 1, 1, 0, 0, 0), update=True)
        with self.assertRaises(ValueError):
            API['payload'](self.entries, '0.2.3', {})


if __name__ == '__main__':
    unittest.main()

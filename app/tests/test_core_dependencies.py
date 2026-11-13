"""Archive/path data guards only. No fake test executable is extracted or run."""
import io
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import core_dependencies as dependencies


class Dependencies(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cycode-dependency-data-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)

    def archive(self, paths):
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as archive:
            archive.writestr('clangd/bin/clangd.exe', b'archive metadata fixture; never extracted')
            for path in paths:
                if isinstance(path, str):
                    entry = zipfile.ZipInfo('entry')
                    entry.filename = entry.orig_filename = path
                else: entry = path
                archive.writestr(entry, b'data')
        data.seek(0)
        return data

    def test_unsafe_archives_are_rejected_before_any_extraction(self):
        for index, path in enumerate(('clangd/../escape', '/absolute', 'clangd/bin/CLANGD.EXE', 'clangd/bin/clangd.exe/file',
                     'clangd/bin/CON.txt', 'clangd/a.', 'clangd/a /file', 'clangd/a:b',
                     'clangd/a\\b', 'clangd/./file', 'other/root', 'clangd/bin', 'clangd/a\x00b')):
            with self.subTest(path=path):
                stage = self.base / ('stage-' + str(index))
                with self.assertRaises(ValueError): dependencies.unpack(self.archive([path]), stage)
                self.assertFalse(stage.exists(), 'The archive must be fully checked before extraction')

    def test_links_and_expansion_budget_are_rejected(self):
        link = zipfile.ZipInfo('clangd/link')
        link.create_system = 3; link.external_attr = (stat.S_IFLNK | 0o777) << 16
        with self.assertRaises(ValueError): dependencies.unpack(self.archive([link]), self.base / 'links')
        with patch.object(dependencies, 'MAX_EXPANDED', 1), self.assertRaises(ValueError):
            dependencies.unpack(self.archive([]), self.base / 'size')
        self.assertFalse((self.base / 'size').exists())

    def test_cancel_before_extraction_keeps_the_stage_empty(self):
        with self.assertRaisesRegex(RuntimeError, 'cancelled'):
            dependencies.unpack(self.archive([]), self.base / 'cancelled', cancelled=lambda: True)
        self.assertFalse((self.base / 'cancelled').exists())

    def test_invalid_managed_pointer_is_preserved_and_reported(self):
        pointer = self.base / 'dependencies/clangd/current.json'
        pointer.parent.mkdir(parents=True)
        for version in ('../escape', 123, '1.2.3/other'):
            data = json.dumps({'format': 'cycode-clangd/1', 'version': version})
            pointer.write_text(data)
            with self.assertRaises(ValueError): dependencies.selection(self.base)
            self.assertEqual(pointer.read_text(), data)
            row = dependencies.inspect(self.base, root=self.base)[0]
            self.assertTrue(row['issue'])

    def test_explicit_override_precedes_private_installation(self):
        # Real ordinary text file is enough for discovery; nothing is executed.
        explicit = self.base / 'selected-tool.txt'; explicit.write_text('not an executable')
        with patch.dict(os.environ, {'CYCODE_CLANGD': str(explicit)}), \
             patch.object(dependencies, 'selection', return_value={'path': 'private', 'source': 'managed'}):
            row = dependencies.inspect(self.base, root=self.base)[0]
            self.assertEqual(row['selection']['source'], 'override')
            self.assertEqual(row['selection']['path'], str(explicit))

    def test_pinned_release_and_confirmation(self):
        lock = dependencies.catalogue()
        self.assertEqual(lock['size'], 29690805)
        self.assertEqual(lock['version'], '23.1.0')
        with self.assertRaisesRegex(ValueError, 'Review the source'):
            dependencies.install(self.base)
        with self.assertRaisesRegex(RuntimeError, 'cancelled'):
            dependencies.install(self.base, accepted=True, cancelled=lambda: True)


if __name__ == '__main__': unittest.main()

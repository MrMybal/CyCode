"""Reviewed documentation and cross-repository help routes; no native/Node process."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

APP = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('cycode_documentation_verify', APP / '.CyDocBuild/verify.py')
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class DocumentationContracts(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.app = Path(temporary.name) / 'app'
        for directory in ('.CyDocBuild', 'Docs', 'src'):
            (self.app / directory).mkdir(parents=True, exist_ok=True)

    def test_current_guides_have_reviewed_coverage(self):
        self.assertTrue(VERIFY.verify_source_coverage())

    def test_changed_source_requires_review_without_rewriting_metadata(self):
        source = self.app / 'Docs/Guide.md'
        source.write_text('Reviewed guide.\n', encoding='utf-8')
        row = dict(source='Docs/Guide.md', kind='developer-reference',
                   sourceSha256=hashlib.sha256(source.read_text('utf-8').encode('utf-8')).hexdigest())
        coverage = self.app / '.CyDocBuild/coverage.json'
        coverage.write_text(json.dumps(dict(format=1, sources=[row])), encoding='utf-8')
        before = coverage.read_bytes()
        VERIFY.verify_source_coverage(self.app)
        source.write_bytes(b'Reviewed guide.\r\n')
        VERIFY.verify_source_coverage(self.app)
        source.write_text('Changed after review.\n', encoding='utf-8')
        with self.assertRaisesRegex(AssertionError, 'Review changed documentation source: Docs/Guide.md'):
            VERIFY.verify_source_coverage(self.app)
        self.assertEqual(coverage.read_bytes(), before)

    def help_fixture(self):
        (self.app / 'src/HelpTopics.h').write_text(
            '{"analysis", "analysis-progress", "persistence"}', encoding='utf-8')
        (self.app / 'src/AnalysisUI.cpp').write_text(
            'beginPanel("Analysis and inspections", "analysis");\n'
            'app_help::open(root_, "app/Documentation", locale_, "core-quality", "inspections", false);\n',
            encoding='utf-8')
        return {(locale, page): VERIFY.References(f'<h2 id="{anchor}">Help</h2>')
                for locale in ('en', 'fr')
                for page, anchor in [('analysis-progress', 'persistence'), ('core-quality', 'inspections')]}

    def test_analysis_panel_and_explicit_inspection_help(self):
        self.assertEqual(VERIFY.verify_native_help(self.help_fixture(), app=self.app), (1, 1))

    def test_unregistered_analysis_panel_is_refused(self):
        parsed = self.help_fixture()
        (self.app / 'src/HelpTopics.h').write_text(
            '{"build", "analysis-progress", "persistence"}', encoding='utf-8')
        with self.assertRaisesRegex(AssertionError, 'panel has no contextual help: analysis'):
            VERIFY.verify_native_help(parsed, app=self.app)

    def test_explicit_inspection_anchor_requires_both_languages(self):
        parsed = self.help_fixture()
        parsed['fr', 'core-quality'] = VERIFY.References('<h2 id="other">Other</h2>')
        with self.assertRaisesRegex(AssertionError, 'fr/core-quality#inspections'):
            VERIFY.verify_native_help(parsed, app=self.app)


if __name__ == '__main__':
    unittest.main()

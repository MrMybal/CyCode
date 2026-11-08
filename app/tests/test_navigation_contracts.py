"""Navigation metadata contracts only; does not import an SDK runtime or run native code."""
import json
from pathlib import Path
import unittest
from jsonschema import Draft202012Validator


class CounterpartContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        entries = json.loads((Path(__file__).resolve().parents[1] / 'CyTool.json').read_text('utf-8'))['operations']
        cls.operation = next(entry for entry in entries if entry['id'] == 'workspace_counterparts')

    def test_read_only_guarded_input(self):
        self.assertEqual(self.operation['permissions'], [])
        valid = Draft202012Validator(self.operation['inputSchema'])
        request = dict(path='Source/Module/Public/Actor.h', expectedRevision=1, expectedDocumentId=2)
        self.assertTrue(valid.is_valid(request))
        for key in request:
            reduced = dict(request)
            del reduced[key]
            self.assertFalse(valid.is_valid(reduced))
        for key in ('expectedRevision', 'expectedDocumentId'):
            for value in (True, -1, 1.5, '1'):
                self.assertFalse(valid.is_valid({**request, key: value}))
        self.assertFalse(valid.is_valid({**request, 'root': 'elsewhere'}))

    def test_bounded_partial_report(self):
        valid = Draft202012Validator(self.operation['outputSchema'])
        report = dict(path='Actor.h', revision=1, documentId=2, checked=12,
                      index=dict(state='ready', revision=7, generation=2), unreadable=0,
                      truncated=False, candidates=[dict(path='Actor.cpp', relation='sibling')])
        self.assertTrue(valid.is_valid(report))
        self.assertTrue(valid.is_valid({**report, 'truncated': True, 'unreadable': 1}))
        self.assertTrue(valid.is_valid({**report, 'candidates': []}))
        self.assertFalse(valid.is_valid({**report, 'checked': 129}))
        self.assertFalse(valid.is_valid({**report, 'unreadable': -1}))
        self.assertFalse(valid.is_valid({**report, 'candidates': report['candidates'] * 129}))
        self.assertFalse(valid.is_valid({**report, 'candidates': [dict(path='Actor.cpp', relation='semantic')]}))
        self.assertTrue(valid.is_valid({**report, 'candidates': [dict(path='include/lib/Actor.hpp', relation='workspace-index')]}))

    def test_explicit_index_observations(self):
        valid = Draft202012Validator(self.operation['outputSchema'])
        base = dict(path='src/Flat.cpp', revision=1, documentId=2, checked=50,
                    unreadable=0, truncated=True, candidates=[])
        for state in ('missing', 'busy', 'stale', 'ready', 'partial', 'changed'):
            report = {**base, 'index': dict(state=state, revision=0, generation=0)}
            self.assertTrue(valid.is_valid(report))
        self.assertFalse(valid.is_valid(base))
        report = {**base, 'index': dict(state='ready', revision=7, generation=2)}
        for key in ('state', 'revision', 'generation'):
            reduced = dict(report['index'])
            del reduced[key]
            self.assertFalse(valid.is_valid({**base, 'index': reduced}))
        for key in ('revision', 'generation'):
            for value in (True, -1, 1.5, '1'):
                self.assertFalse(valid.is_valid({**report, 'index': {**report['index'], key: value}}))
        self.assertFalse(valid.is_valid({**report, 'index': {**report['index'], 'state': 'complete'}}))


class IncludeContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        entries = json.loads((Path(__file__).resolve().parents[1] / 'CyTool.json').read_text('utf-8'))['operations']
        cls.operation = next(entry for entry in entries if entry['id'] == 'workspace_include')

    def test_guarded_caret_input(self):
        self.assertEqual(self.operation['permissions'], [])
        valid = Draft202012Validator(self.operation['inputSchema'])
        request = dict(path='Source/Module/Private/Actor.cpp', expectedRevision=3,
                       expectedDocumentId=2, position=dict(line=1, character=15))
        self.assertTrue(valid.is_valid(request))
        for key in request:
            reduced = dict(request)
            del reduced[key]
            self.assertFalse(valid.is_valid(reduced))
        for key in ('line', 'character'):
            for value in (True, -1, '1', 1.5):
                self.assertFalse(valid.is_valid({**request, 'position': {**request['position'], key: value}}))
        self.assertFalse(valid.is_valid({**request, 'target': 'caller-supplied.h'}))
        self.assertFalse(valid.is_valid({**request, 'root': 'elsewhere'}))

    def test_literal_and_bounded_observations(self):
        valid = Draft202012Validator(self.operation['outputSchema'])
        report = dict(path='Actor.cpp', revision=3, documentId=2, position=dict(line=1, character=15),
                      include=dict(target='lib/Actor.inc', angled=False,
                                   range=dict(start=dict(line=1, character=10), end=dict(line=1, character=23))),
                      index=dict(state='ready', revision=7, generation=2), checked=2,
                      unreadable=0, truncated=False,
                      candidates=[dict(path='include/lib/Actor.inc', relation='workspace-index')])
        self.assertTrue(valid.is_valid(report))
        self.assertTrue(valid.is_valid({**report, 'include': {**report['include'], 'angled': True}}))
        continued = {**report, 'position': dict(line=0, character=0),
                     'include': dict(target='été.h', angled=False,
                                     range=dict(start=dict(line=2, character=3), end=dict(line=2, character=8)))}
        self.assertTrue(valid.is_valid(continued))  # Source range stays on the actual header line.
        self.assertTrue(valid.is_valid({**report, 'candidates': [dict(path='lib/Actor.inc', relation='source-relative')]}))
        for target in ('', 'x' * 1025):
            self.assertFalse(valid.is_valid({**report, 'include': {**report['include'], 'target': target}}))
        self.assertFalse(valid.is_valid({**report, 'include': {**report['include'], 'angled': 1}}))
        self.assertFalse(valid.is_valid({**report, 'candidates': report['candidates'] * 129}))
        self.assertFalse(valid.is_valid({**report, 'checked': 129}))
        self.assertFalse(valid.is_valid({**report, 'candidates': [dict(path='Actor.h', relation='paired-directory')]}))
        for key in report:
            reduced = dict(report)
            del reduced[key]
            self.assertFalse(valid.is_valid(reduced))


class WorkspaceFilePageContracts(unittest.TestCase):
    def test_rebuild_generation_is_required(self):
        entries = json.loads((Path(__file__).resolve().parents[1] / 'CyTool.json').read_text('utf-8'))['operations']
        operation = next(entry for entry in entries if entry['id'] == 'workspace_files')
        self.assertEqual(operation['permissions'], [])
        valid = Draft202012Validator(operation['outputSchema'])
        first = dict(files=['Source/Foo.cpp'], truncated=True, nextOffset=1, total=2,
                     indexRevision=7, generation=2)
        self.assertTrue(valid.is_valid(first))
        self.assertTrue(valid.is_valid({**first, 'generation': 3}))
        reduced = dict(first)
        del reduced['generation']
        self.assertFalse(valid.is_valid(reduced))
        for value in (0, -1, True, '2', 2.5):
            self.assertFalse(valid.is_valid({**first, 'generation': value}))


if __name__ == '__main__':
    unittest.main()

"""Public snippet JSON contracts only; no application, SDK runtime or native process."""
import importlib.util
import json
from pathlib import Path
import unittest

from jsonschema import Draft202012Validator

APP = Path(__file__).resolve().parents[1]


class SnippetContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('snippet_contract_data', APP / 'contracts.py')
        cls.contracts = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.contracts)  # Schema data, without tool.py/runtime.
        cls.operations = {entry['id']: entry for entry in json.loads(
            (APP / 'CyTool.json').read_text('utf-8'))['operations']}

    def test_generated_schemas_match_source(self):
        self.assertEqual(len(self.operations), len(self.contracts.OPERATIONS))
        for identity, _, _, inputs, outputs, _ in self.contracts.OPERATIONS:
            with self.subTest(operation=identity):
                entry = self.operations[identity]
                self.assertEqual(entry['inputSchema'], inputs)
                self.assertEqual(entry['outputSchema'], outputs)
                Draft202012Validator.check_schema(inputs)
                Draft202012Validator.check_schema(outputs)

    def test_read_states_include_zero_generation(self):
        missing = dict(available=True, editable=True, valid=True, revision='absent',
                       generation=1, text='{\n}\n', error='')
        states = [missing,
                  {**missing, 'available': False, 'editable': False, 'valid': False,
                   'revision': 'disabled', 'generation': 0, 'text': ''},
                  {**missing, 'editable': False, 'valid': False, 'revision': '',
                   'generation': 0, 'text': '', 'error': 'Cannot read storage'},
                  {**missing, 'valid': False, 'revision': 'f' * 64,
                   'text': '{broken', 'error': 'Malformed JSON'}]
        for operation in ('snippets_user_read', 'snippets_user_save'):
            validate = Draft202012Validator(self.operations[operation]['outputSchema'])
            for state in states:
                with self.subTest(operation=operation, revision=state['revision']):
                    self.assertTrue(validate.is_valid(state))
            for generation in (-1, True, '0'):
                self.assertFalse(validate.is_valid({**missing, 'generation': generation}))

    def test_save_revision_and_fixed_location(self):
        validate = Draft202012Validator(self.operations['snippets_user_save']['inputSchema'])
        for revision in ('absent', '0123456789abcdef' * 4):
            self.assertTrue(validate.is_valid(dict(text='{}', expectedRevision=revision)))
        for revision in ('', 'disabled', 'absent\n', 'f' * 63, 'f' * 65,
                         'F' * 64, 'g' * 64, 'f' * 64 + '\n', 1, True):
            with self.subTest(revision=revision):
                self.assertFalse(validate.is_valid(dict(text='{}', expectedRevision=revision)))
        self.assertFalse(validate.is_valid(dict(text=' ' * 262145, expectedRevision='absent')))
        self.assertFalse(validate.is_valid(dict(text='{}', expectedRevision='absent', path='outside')))
        read = Draft202012Validator(self.operations['snippets_user_read']['inputSchema'])
        self.assertTrue(read.is_valid({}))
        self.assertFalse(read.is_valid({'path': 'outside'}))

    def test_save_permission_and_scheduler_metadata(self):
        read = self.operations['snippets_user_read']
        save = self.operations['snippets_user_save']
        self.assertEqual(read['permissions'], [])
        self.assertEqual(save['permissions'], ['settings.write'])
        self.assertFalse(save['canCancel'])
        self.assertEqual(save['exclusiveGroups'], ['user-settings'])
        # These are declarations; this does not exercise scheduler/permission enforcement.

    def test_selection_draft_contract_is_read_only(self):
        operation = self.operations['snippets_user_draft']
        self.assertEqual(operation['permissions'], [])
        validate = Draft202012Validator(operation['inputSchema'])
        request = dict(path='main.verse', expectedRevision=1,
                       selection={'start': {'line': 0, 'character': 0},
                                  'end': {'line': 1, 'character': 0}})
        self.assertTrue(validate.is_valid(request))
        for key in request:
            candidate = dict(request)
            del candidate[key]
            self.assertFalse(validate.is_valid(candidate))
        self.assertFalse(validate.is_valid({**request, 'directory': 'outside'}))
        self.assertFalse(validate.is_valid({**request, 'expectedRevision': True}))
        self.assertFalse(validate.is_valid({**request, 'selection': {'start': request['selection']['start']}}))
        output = dict(catalogue=dict(available=True, editable=True, valid=True,
                      revision='absent', generation=1, text='{}', error=''),
                      draft='{"Selection snippet":{"prefix":"snippet","body":"x$0","scope":"verse"}}',
                      name='Selection snippet', path='main.verse', revision=1)
        result = Draft202012Validator(operation['outputSchema'])
        self.assertTrue(result.is_valid(output))
        self.assertFalse(result.is_valid({**output, 'draft': ' ' * 262145}))


if __name__ == '__main__':
    unittest.main()

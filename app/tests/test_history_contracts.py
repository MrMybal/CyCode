"""Shared native history/commit contracts. Data only; never starts Core."""
import json
from pathlib import Path
import unittest
from jsonschema import Draft202012Validator


class HistoryContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ops = {row['id']: row for row in json.loads(
            (Path(__file__).resolve().parents[1] / 'CyTool.json').read_text('utf-8'))['operations']}

    def valid(self, operation, value, output=False):
        return Draft202012Validator(self.ops[operation][
            'outputSchema' if output else 'inputSchema']).is_valid(value)

    def test_prepare_requires_all_live_guards(self):
        request = dict(id='issued', direction='undo', expectedHistoryRevision=4,
                       documents=[dict(path='Large.log', expectedDocumentId=2, expectedRevision=8)])
        self.assertTrue(self.valid('changes_history_prepare', request))
        for field in ('id', 'direction', 'expectedHistoryRevision', 'documents'):
            self.assertFalse(self.valid('changes_history_prepare',
                                       {key: value for key, value in request.items() if key != field}))
        for field in ('path', 'expectedDocumentId', 'expectedRevision'):
            self.assertFalse(self.valid('changes_history_prepare', dict(request,
                documents=[{key: value for key, value in request['documents'][0].items() if key != field}])))
        for direction in ('apply', '', 'Undo'):
            self.assertFalse(self.valid('changes_history_prepare', dict(request, direction=direction)))
        self.assertFalse(self.valid('changes_history_prepare', dict(request, expectedHistoryRevision=0)))
        self.assertFalse(self.valid('changes_history_prepare', dict(request,
            documents=[dict(path='bad\0.log', expectedDocumentId=2, expectedRevision=8)])))

    def test_prepare_is_read_only_but_not_cancellable_during_native_preflight(self):
        for name in ('changes_status', 'changes_history_status', 'changes_history_prepare'):
            self.assertEqual(self.ops[name]['permissions'], [])
        self.assertFalse(self.ops['changes_history_prepare']['canCancel'])
        for name in ('changes_apply', 'changes_undo'):
            self.assertIn('workspace.write', self.ops[name]['permissions'])
            self.assertFalse(self.ops[name]['canCancel'])

    def test_receipt_is_a_committed_snapshot_not_an_optional_partial_document(self):
        document = dict(path='Large.log', id=2, revision=9, dirty=True, bytes=5000000, storage='paged')
        receipt = dict(transaction='replay', action='undo', historyRevision=5,
                       documents=[document], notificationsComplete=False)
        plan = dict(id='replay', title='Undo', state='applied', files=[], editCount=0,
                    history=dict(source='original', direction='undo', expectedHistoryRevision=4), commit=receipt)
        self.assertTrue(self.valid('changes_apply', plan, True))
        self.assertTrue(self.valid('changes_status', plan, True))
        for field in document:
            broken = dict(receipt, documents=[{key: value for key, value in document.items() if key != field}])
            self.assertFalse(self.valid('changes_apply', dict(plan, commit=broken), True))
        self.assertFalse(self.valid('changes_apply', dict(plan, commit=dict(receipt, action='preview')), True))
        self.assertFalse(self.valid('changes_apply', dict(plan, commit=dict(receipt, notificationsComplete='yes')), True))
        self.assertTrue(self.valid('changes_status', {key: value for key, value in plan.items()
                                                     if key not in ('commit', 'history')}, True))

    def test_history_status_exposes_retention_and_nullable_heads(self):
        status = dict(revision=1, undoCount=0, redoCount=0, undo=None, redo=None,
                      retentionLimit=256, dropped=0)
        self.assertTrue(self.valid('changes_history_status', status, True))
        self.assertTrue(self.valid('changes_history_status', dict(status,
            undoCount=1, undo=dict(id='original', title='Edit', files=['Large.log'])), True))
        self.assertFalse(self.valid('changes_history_status', dict(status, retentionLimit=0), True))
        self.assertFalse(self.valid('changes_history_status', dict(status, undo=dict(id='original')), True))
        self.assertFalse(self.valid('changes_history_status', dict(status, revision=0), True))


if __name__ == '__main__':
    unittest.main()

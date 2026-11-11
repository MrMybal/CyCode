"""Paged storage/public transaction contracts; data only, never start Core."""
import json
from pathlib import Path
import unittest
from jsonschema import Draft202012Validator
class CapacityContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ops={row['id']:row for row in json.loads((Path(__file__).resolve().parents[1]/'CyTool.json').read_text('utf-8'))['operations']}
    def valid(self,name,value,output=False):
        return Draft202012Validator(self.ops[name]['outputSchema' if output else 'inputSchema']).is_valid(value)
    def test_paged_ranges_and_live_guards(self):
        self.assertTrue(self.valid('documents_read_page',{'path':'Large.log','length':262144}))
        for args in [{'path':'Large.log','offset':10**80},{'path':'Large.log','length':3},{'path':'Large.log','offset':-1},{'path':'Large.log','length':262145},{'path':'bad\0.cpp'}]:
            self.assertFalse(self.valid('documents_read_page',args))
        self.assertFalse(self.valid('documents_prepare_byte_edit',{'path':'Large.log','offset':0,'length':0,'text':'x'}))
        self.assertTrue(self.valid('documents_prepare_byte_edit',{'path':'Large.log','offset':90000000,'length':1,'text':'été','expectedRevision':7}))
    def test_mutation_permission_and_cancellation_truth(self):
        for name in ('documents_read_page','documents_prepare_byte_edit'):
            self.assertEqual(self.ops[name]['permissions'],[])
        for name in ('documents_save_paged','documents_close_paged','changes_apply','changes_undo'):
            self.assertIn('workspace.write',self.ops[name]['permissions'])
            self.assertFalse(self.ops[name]['canCancel'])
    def test_independent_preview_byte_cursors(self):
        self.assertTrue(self.valid('changes_preview',{'id':'issued','path':'Large.log','beforeOffset':0,'afterOffset':4,'length':4096}))
        self.assertFalse(self.valid('changes_preview',{'id':'issued','path':'Large.log','afterOffset':-1}))
    def test_paged_metadata_is_not_a_partial_legacy_document(self):
        row=dict(path='Large.log',id=1,revision=1,dirty=False,bytes=100000000,storage='paged',positionEncoding='utf8-bytes',text='été',offset=0,nextOffset=5,hasMore=True)
        self.assertTrue(self.valid('documents_read_page',row,True))
        self.assertFalse(self.valid('documents_read',row,True))
        self.assertFalse(self.valid('documents_read_page',{**row,'positionEncoding':'utf16'},True))
if __name__=='__main__':unittest.main()

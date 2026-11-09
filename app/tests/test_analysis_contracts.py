"""Semantic coverage contracts: data-only, no native process or SDK host."""
import json
from pathlib import Path
import unittest
from jsonschema import Draft202012Validator

class AnalysisContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ops=json.loads((Path(__file__).resolve().parents[1]/'CyTool.json').read_text('utf-8'))['operations']
        cls.ops={op['id']:op for op in ops}
    def valid(self,operation,value,output=False):
        return Draft202012Validator(self.ops[operation]['outputSchema' if output else 'inputSchema']).is_valid(value)
    def test_unbounded_explicit_file_count(self):
        self.assertTrue(self.valid('analysis_prepare',{'paths':[f'Source/{i}.cpp' for i in range(200)]}))
        self.assertFalse(self.valid('analysis_prepare',{'paths':[]}))
        self.assertFalse(self.valid('analysis_prepare',{'paths':['A.cpp','A.cpp']}))
    def test_inventory_resume_and_conflicting_modes(self):
        for value in ({'scope':'compilation-database','batchSize':0},
                      {'scope':'compilation-database','pathPrefix':'Engine/Source'},
                      {'continuation':'analysis-7','batchSize':64}):
            self.assertTrue(self.valid('analysis_prepare',value),value)
        for value in ({},{'continuation':''},{'paths':['A.cpp'],'continuation':'analysis-7'},
                      {'scope':'compilation-database','paths':['A.cpp']},{'pathPrefix':'Engine'},
                      {'continuation':'x','pathPrefix':'Engine'},{'continuation':'x','batchSize':4097}):
            self.assertFalse(self.valid('analysis_prepare',value),value)
    def test_history_is_paged_not_silently_truncated(self):
        self.assertTrue(self.valid('analysis_results',{'session':'analysis-1','offset':128,'limit':256}))
        self.assertFalse(self.valid('analysis_results',{'session':'analysis-1','offset':-1}))
        self.assertFalse(self.valid('analysis_results',{'session':'analysis-1','limit':257}))
        row=dict(path='A.cpp',revision=1,diagnostics='clean',errors=0)
        page=dict(session='analysis-1',documents=[row],offset=128,nextOffset=129,available=160,total=160,hasMore=True)
        self.assertTrue(self.valid('analysis_results',page,True))
        self.assertFalse(self.valid('analysis_results',{**page,'documents':[dict(row,diagnostics='complete')]},True))
    def test_traversal_completion_does_not_claim_index_completeness(self):
        coverage={key:0 for key in ('diagnosticClean','diagnosticErrors','diagnosticUnknown','commands','translationUnits','outOfScope','observed','remaining')}
        result=dict(state='pending',phase='pending',completed=0,total=37557,progress=0,error='',session='analysis-1',
            coverage=coverage,documents=[],scope='compilation-database',wholeProjectIndexCompleteness='unknown',continuation='analysis-1',nextOffset=0)
        self.assertTrue(self.valid('analysis_prepare',result,True))
        self.assertFalse(self.valid('analysis_prepare',dict(result,wholeProjectIndexCompleteness='complete'),True))
        self.assertFalse(self.valid('analysis_prepare',dict(result,documents=[dict(path='A.cpp',revision=1)]),True))
    def test_sdk_retains_preparation_ownership_and_read_permissions(self):
        for name in ('analysis_prepare','analysis_status','analysis_results'):
            self.assertEqual(self.ops[name]['permissions'],[])
        self.assertEqual(self.ops['analysis_prepare']['exclusiveGroups'],self.ops['language_rename']['exclusiveGroups'])
        self.assertTrue(self.ops['analysis_prepare']['supportsProgress'])

    def test_actual_clangd_hierarchy_capabilities_are_typed_without_open_schema(self):
        # Real MCP language_configure previously failed on these clangd fields.
        methods=self.ops['language_status']['outputSchema']['properties']['servers']['properties']['cpp']['properties']['methods']
        validator=Draft202012Validator(methods)
        actual={'definition':True,'references':True,'callHierarchy':True,'typeHierarchy':True}
        self.assertTrue(validator.is_valid(actual))
        self.assertFalse(validator.is_valid(dict(actual,callHierarchy='yes')))
        self.assertFalse(validator.is_valid(dict(actual,arbitraryCapability=True)))

if __name__=='__main__':unittest.main()

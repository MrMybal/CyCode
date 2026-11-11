"""Schema and isolated SDK registry tests; no Core, LLVM or debugger launch."""
import json,os,subprocess,sys,tempfile,unittest
from pathlib import Path
from jsonschema import Draft202012Validator
APP=Path(__file__).resolve().parents[1]
class CppInspectionContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.operation=next(op for op in json.loads((APP/'CyTool.json').read_text('utf-8'))['operations'] if op['id']=='analysis_inspect')
    def valid(self,value):return Draft202012Validator(self.operation['inputSchema']).is_valid(value)
    def test_requires_saved_revision_and_real_context(self):
        base={'path':'Source/Test.cpp','expectedRevision':1,'compilationDatabase':'.cycode/compile_commands.json'}
        self.assertTrue(self.valid(base))
        for field in base:self.assertFalse(self.valid({k:v for k,v in base.items() if k!=field}))
        self.assertFalse(self.valid(dict(base,expectedRevision=0)))
        for extra in ({'fix':True},{'arguments':['-fix']},{'profile':'arbitrary-checks'},{'timeoutSeconds':0},{'timeoutSeconds':901}):
            self.assertFalse(self.valid({**base,**extra}))
    def test_process_permission_and_cancellation(self):
        self.assertEqual(self.operation['permissions'],['process.execute'])
        self.assertTrue(self.operation['canCancel'])
        schema=self.operation['outputSchema']['properties']
        self.assertEqual(schema['sourceWritten'],{'const':False})
        self.assertEqual(schema['positionEncoding'],{'const':'utf8-byte-columns'})
        self.assertIn('stale',schema['status']['enum'])
    def test_private_python_uses_facade_owned_contracts(self):
        # Embedded Python has an installation app in ._pth. Executing a newer
        # facade must register its own complete manifest, not that old app's.
        code='import pathlib,runpy,sys; module=runpy.run_path(sys.argv[1]); runtime=module["create_runtime"](pathlib.Path(sys.argv[2])); runtime.start(); runtime.close()'
        with tempfile.TemporaryDirectory(prefix='cycode-sdk-registry-') as directory:
            result=subprocess.run([sys.executable,'-B','-c',code,str(APP/'tool.py'),directory],capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stderr)
if __name__=='__main__':unittest.main()

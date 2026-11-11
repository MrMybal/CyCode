"""Data-only checks; never spawn CLI, clangd or any native fixture."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock

path=Path(__file__).resolve().parents[1]/'scripts/benchmark_semantic.py'
spec=importlib.util.spec_from_file_location('semantic_baseline',path)
api=importlib.util.module_from_spec(spec);spec.loader.exec_module(api)

class BaselineDataTests(unittest.TestCase):
    def test_position_counts_supplementary_characters_in_utf16(self):
        self.assertEqual(api.position('// 🛠 été\nint value;', 'value'), {'line':1,'character':4})
        self.assertEqual(api.position('🛠 value', 'value'), {'line':0,'character':3})

    def test_corpus_is_path_independent_and_compiler_context_is_explicit(self):
        with tempfile.TemporaryDirectory() as tmp:
            a=Path(tmp)/'a';b=Path(tmp)/'b'
            first=api.corpus(a,3);second=api.corpus(b,3)
            self.assertEqual(first,second)
            commands=json.loads((a/'compile_commands.json').read_text())
            self.assertEqual(len(commands),4)
            for item in commands:
                self.assertIn('-DCYCODE_BENCH_FACTOR=2',item['arguments'])
                self.assertEqual(item['directory'],str(a))
            with self.assertRaises(FileExistsError):api.corpus(a,3)

    def test_prepare_only_executes_no_native_process(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(api.subprocess,'Popen',side_effect=AssertionError('native start')), patch.object(api.subprocess,'check_output',side_effect=AssertionError('native query')):
            output=Path(tmp)/'report'
            self.assertEqual(api.main(['--prepare-only','--output',str(output),'--units','2']),0)
            report=json.loads((output/'baseline.json').read_text())
            self.assertEqual(report['execution'],'not-run: source-data-only')
            self.assertEqual(report['runs'],[])

    def test_local_native_execution_is_refused_before_writing(self):
        with tempfile.TemporaryDirectory() as tmp, patch.dict(api.os.environ,{},clear=True):
            output=Path(tmp)/'refused'
            with self.assertRaises(SystemExit):api.main(['--ci','--cli','missing','--clangd','missing','--output',str(output)])
            self.assertFalse(output.exists())

    def test_bad_handshake_closes_owned_host_without_native_execution(self):
        process=Mock()
        process.stdin=io.BytesIO()
        process.stdout=io.BytesIO(b'{"event":"unexpected"}\n')
        process.wait.return_value=0
        with tempfile.TemporaryDirectory() as tmp, patch.object(api.subprocess,'Popen',return_value=process):
            with self.assertRaisesRegex(RuntimeError,'Unexpected native handshake'):
                api.Host(Path('mock-cli'),Path(tmp),Path(tmp)/'stderr.log')
        self.assertTrue(process.stdin.closed)
        self.assertTrue(process.stdout.closed)
        process.wait.assert_called_once_with(timeout=15)
        process.kill.assert_not_called()

    def test_member_completion_uses_backend_insertion_spelling(self):
        self.assertTrue(api.has_member_completion([{'label':' value : int','filterText':'value'}],'value'))
        self.assertTrue(api.has_member_completion([{'label':' value'}],'value'))
        self.assertTrue(api.has_member_completion([{'label':'display','textEdit':{'newText':'value'}}],'value'))
        self.assertFalse(api.has_member_completion([{'label':'value2','filterText':'value2'}],'value'))
        self.assertFalse(api.has_member_completion([{'label':'value','filterText':'different'}],'value'))

    def test_real_project_uses_actual_export_and_preserves_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'core';(root/'common').mkdir(parents=True)
            (root/'common/api-version.txt').write_text('1')
            (root/'app/src').mkdir(parents=True);(root/'app/include/cycode').mkdir(parents=True)
            source=root/'app/src/cli.cpp';source.write_text('engine.dispatch("inspect");',encoding='utf-8')
            (root/'app/include/cycode/Engine.h').write_text('class Engine {};')
            database=root/'actual-commands.json'
            database.write_text(json.dumps([{'directory':str(root),'file':'app/src/cli.cpp','arguments':['clang++','-DREAL_FLAG=1','-c','app/src/cli.cpp']}]))
            before=source.read_bytes()
            with patch.object(api.subprocess,'Popen',side_effect=AssertionError('native process')):
                description,probe=api.real_core_project(root,database)
            self.assertEqual(description['kind'],'real-native-core-cmake')
            self.assertEqual(description['translationUnits'],1)
            self.assertEqual(description['sourceFiles'],2)
            self.assertEqual(probe['member'],'dispatch')
            self.assertEqual(source.read_bytes(),before)
            self.assertFalse((root/'compile_commands.json').exists())

    def test_statistics_preserve_raw_samples(self):
        report=api.summarize([10,1,8,2,5])
        self.assertEqual(report['samplesMilliseconds'],[10,1,8,2,5])
        self.assertEqual(report['medianMilliseconds'],5)
        self.assertEqual(report['p95Milliseconds'],10)

if __name__=='__main__':unittest.main()

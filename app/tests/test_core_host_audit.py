"""Local failure evidence only: never starts a native or mocked executable."""
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

SPEC=importlib.util.spec_from_file_location('core_host',Path(__file__).resolve().parents[1]/'core_host.py')
HOST=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(HOST)

class HostFailureAudit(unittest.TestCase):
    def test_bounded_private_log_retained_without_endpoint_credentials(self):
        with tempfile.TemporaryDirectory() as temporary,patch.object(HOST.subprocess,'Popen',side_effect=AssertionError('No process allowed')):
            root=Path(temporary);folder=root/'private';folder.mkdir()
            (folder/'native.log').write_bytes(b'x'*80000)
            (folder/'native.json').write_text('{"token":"never-copy-this-secret"}',encoding='utf-8')
            owner=object.__new__(HOST.ManagedCore)
            owner.data=root;owner.folder=SimpleNamespace(name=str(folder))
            owner.process=SimpleNamespace(returncode=19);owner.executable=Path(__file__);owner.workspace=root
            report_dir=owner._preserve_failure()
            report=json.loads((report_dir/'report.json').read_text('utf-8'))
            self.assertEqual(report['exitCode'],19)
            self.assertEqual(report['cause'],'undetermined')
            self.assertFalse(report['automaticRestart'])
            self.assertTrue(report['logTruncated'])
            self.assertEqual((report_dir/'native.log').stat().st_size,65536)
            self.assertFalse((report_dir/'native.json').exists())
            self.assertNotIn('never-copy-this-secret',(report_dir/'report.json').read_text('utf-8'))

if __name__=='__main__':unittest.main()

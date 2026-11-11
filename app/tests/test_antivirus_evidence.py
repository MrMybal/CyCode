"""Evidence policy only; no executables, native hosts or network connections."""
import importlib.util
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC=importlib.util.spec_from_file_location('antivirus_evidence',Path(__file__).resolve().parents[1]/'scripts/antivirus_evidence.py')
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)

class AntivirusEvidence(unittest.TestCase):
    def setUp(self):
        self.process=patch.object(subprocess,'Popen',side_effect=AssertionError('No process allowed'));self.process.start()
        self.network=patch.object(socket,'socket',side_effect=AssertionError('No network allowed'));self.network.start()
        self.addCleanup(self.process.stop);self.addCleanup(self.network.stop)
    def test_missing_original_is_never_recreated_or_substituted(self):
        with tempfile.TemporaryDirectory() as directory:
            missing=Path(directory)/'cycode_cli.exe'
            report=MODULE.collect(missing,'PDM:Trojan.Win32.Generic','2026-11-09 16:07 Europe/Paris')
            self.assertFalse(missing.exists())
            self.assertFalse(report['artifact']['available'])
            self.assertIsNone(report['artifact']['sha256'])
            self.assertFalse(report['uploaded'])
            self.assertFalse(report['falsePositiveEstablished'])
    def test_original_hash_must_match_not_a_new_build(self):
        with tempfile.TemporaryDirectory() as directory:
            data=Path(directory)/'ordinary-text.txt';data.write_text('plain text evidence',encoding='utf-8')
            actual=hashlib.sha256(data.read_bytes()).hexdigest()
            report=MODULE.collect(data,'detection','time',known_sha256=actual)
            self.assertEqual(report['artifact']['sha256'],actual)
            with self.assertRaises(ValueError):MODULE.collect(data,'detection','time',known_sha256='0'*64)
            with self.assertRaises(ValueError):MODULE.collect(data,'detection','time',known_sha256='wrong-hash')
    def test_report_payload_and_private_paths_are_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);raw=root/'owner-export.txt';raw.write_text('secret-token-for-test',encoding='utf-8')
            report=MODULE.collect(root/'missing.exe','detection','time',reports=[raw])
            output=root/'dossier';MODULE.write_dossier(report,output)
            contents=''.join(p.read_text('utf-8') for p in output.iterdir())
            self.assertNotIn('secret-token-for-test',contents)
            self.assertNotIn(str(root),contents)
            self.assertEqual({p.name for p in output.iterdir()},{'report.json','vendor-request.txt'})
            self.assertFalse(report['reports'][0]['contentIncluded'])
            with self.assertRaises(FileExistsError):MODULE.write_dossier(report,output)
    def test_known_hash_can_be_recorded_without_restoring_quarantine(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'missing.exe'
            report=MODULE.collect(path,'detection','time',known_sha256='A'*64)
            self.assertEqual(report['artifact']['sha256'],'a'*64)
            self.assertEqual(report['artifact']['hashOrigin'],'antivirus-report')
            self.assertFalse(path.exists())


class KasperskyEventEvidence(unittest.TestCase):
    def event(self,root,name='avp.xml',digest='A'*64,process='cycode_cli.exe',provider='avp',duplicate=False):
        path=root/name
        fields=f'ProcessSha256: "{digest}"\nProcessPath: "c:\\users\\PRIVATE_USER\\{process}"\nProcessCommandLine: "SECRET_COMMAND_TOKEN"\nBases: "2026.11.09 10:03:00:000"\n'
        if duplicate:fields+=f'ProcessSha256: "{digest}"\n'
        fields=fields.replace('\\n','\n')
        path.write_text(f'<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event"><System><Provider Name="{provider}"/><EventID>4662</EventID><TimeCreated SystemTime="2026-11-09T15:07:39Z"/><EventRecordID>123</EventRecordID></System><EventData><Data>{fields}</Data></EventData></Event>',encoding='utf-8')
        return path
    def test_exact_missing_original_hash_recovered_without_private_event_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);event=self.event(root)
            report=MODULE.collect(root/'cycode_cli.exe','PDM:Trojan.Win32.Generic','time',kaspersky_events=[event],antivirus_version='21.26.4.406')
            self.assertEqual(report['artifact']['sha256'],'a'*64)
            self.assertEqual(report['artifact']['hashOrigin'],'kaspersky-event-export')
            self.assertEqual(report['antivirusVersion'],'21.26.4.406')
            self.assertEqual(report['kasperskyEvents'][0]['recordId'],'123')
            MODULE.write_dossier(report,root/'dossier')
            result=''.join(p.read_text('utf-8') for p in (root/'dossier').iterdir())
            self.assertNotIn('PRIVATE_USER',result);self.assertNotIn('SECRET_COMMAND_TOKEN',result)
            self.assertFalse((root/'cycode_cli.exe').exists())
    def test_mixed_builds_and_conflicting_hash_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);one=self.event(root);two=self.event(root,'other.xml',digest='B'*64)
            with self.assertRaises(ValueError):MODULE.collect(root/'cycode_cli.exe','detection','time',kaspersky_events=[one,two])
            with self.assertRaises(ValueError):MODULE.collect(root/'cycode_cli.exe','detection','time',known_sha256='0'*64,kaspersky_events=[one])
    def test_unrelated_provider_filename_duplicate_and_entities_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for options in ({'provider':'unrelated'},{'process':'other.exe'},{'duplicate':True}):
                with self.assertRaises(ValueError):MODULE.parse_kaspersky_event(self.event(root,**options),'cycode_cli.exe')
            hostile=root/'hostile.xml';hostile.write_text('<!DOCTYPE Event [<!ENTITY x "unwanted">]><Event/>',encoding='utf-8')
            with self.assertRaises(ValueError):MODULE.parse_kaspersky_event(hostile,'cycode_cli.exe')
    def test_malformed_or_oversized_hash_is_not_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            with self.assertRaises(ValueError):MODULE.parse_kaspersky_event(self.event(root,digest='A'*65),'cycode_cli.exe')
            large=root/'large.xml';large.write_text('x'*(1024*1024+1),encoding='utf-8')
            with self.assertRaises(ValueError):MODULE.parse_kaspersky_event(large,'cycode_cli.exe')


if __name__=='__main__':unittest.main()

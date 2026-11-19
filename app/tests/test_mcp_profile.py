"""Legacy job-data migration only; no SDK model or native process is launched."""
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from scripts.migrate_legacy_jobs import migrate


class LegacyMigration(unittest.TestCase):
    def test_original_records_and_credentials_survive_without_replay(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);database=root/'state.sqlite3'
            connection=sqlite3.connect(database)
            connection.execute('CREATE TABLE records(kind TEXT,id TEXT,value TEXT,PRIMARY KEY(kind,id))')
            records={'clients':{'clientId':'alice','tokenHash':hashlib.sha256(b'private-token').hexdigest(),'permissions':['workspace.write']},
                'sessions':{'sessionId':'task','clientId':'alice','closed':False},
                'jobs':{'jobId':'issued','clientId':'alice','sessionId':'task','state':'Running','operationId':'documents_replace',
                    'parameters':{'text':'unsaved content'},'outputs':{'path':'original-artifact'},'error':None}}
            for kind,value in records.items(): connection.execute('INSERT INTO records VALUES(?,?,?)',(kind,kind,json.dumps(value)))
            connection.commit();connection.close()
            before=database.read_bytes()
            self.assertTrue(migrate(root))
            result=json.loads((root/'legacy-import.json').read_text('utf-8'))
            self.assertFalse(result['actionsReplayed'])
            for kind,value in records.items(): self.assertEqual(result[kind],[value])
            self.assertEqual(database.read_bytes(),before)
            self.assertFalse(migrate(root))

    def test_live_legacy_owner_prevents_conversion(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            with sqlite3.connect(root/'state.sqlite3') as connection:
                connection.execute('CREATE TABLE records(kind TEXT,id TEXT,value TEXT)')
            connection.close()
            lease=sqlite3.connect(root/'runtime.lock.sqlite3')
            try:
                lease.execute('BEGIN EXCLUSIVE')
                with self.assertRaises(RuntimeError):migrate(root)
                self.assertFalse((root/'legacy-import.json').exists())
            finally:lease.close()

if __name__=='__main__':unittest.main()

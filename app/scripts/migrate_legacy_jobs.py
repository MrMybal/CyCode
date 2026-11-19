"""Snapshot old SQLite jobs with Python's stdlib; no old SDK import or action replay."""
import argparse
import json
from pathlib import Path
import sqlite3
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.install_core_agents import ordinary, atomic


def migrate(directory):
    root = ordinary(directory)
    source, destination = root / 'state.sqlite3', root / 'legacy-import.json'
    if destination.exists() or not source.is_file(): return False
    # Read the current SQLite snapshot, including committed WAL records. Keep all
    # old databases, artifacts and credentials unchanged; never execute old jobs.
    ordinary(source)
    lease = None
    old_lock = ordinary(root / 'runtime.lock.sqlite3')
    if old_lock.exists():
        lease = sqlite3.connect(old_lock.resolve().as_uri() + '?mode=rw', uri=True, timeout=0)
        try: lease.execute('BEGIN EXCLUSIVE')
        except BaseException: lease.close(); raise RuntimeError('Stop the old automation owner before migrating its jobs')
    connection = sqlite3.connect(source.resolve().as_uri() + '?mode=ro', uri=True)
    try:
        connection.execute('PRAGMA query_only=ON')
        connection.execute('BEGIN')
        groups = {kind: [] for kind in ('jobs', 'clients', 'sessions')}
        for kind, text in connection.execute("SELECT kind,value FROM records WHERE kind IN ('jobs','clients','sessions')"):
            groups[kind].append(json.loads(text))
        record = {'format': 'cycode-legacy-jobs/1', 'source': 'state.sqlite3', 'actionsReplayed': False, **groups}
        data = json.dumps(record, ensure_ascii=False, allow_nan=False).encode('utf-8')
        if len(data) > 128 * 1024 * 1024:
            raise ValueError('Legacy job snapshot exceeds the migration budget; export selected records first')
        atomic(destination, data, None)
    finally:
        connection.close()
        if lease is not None: lease.rollback(); lease.close()
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state-dir', type=Path, required=True)
    print(json.dumps({'importCreated': migrate(parser.parse_args().state_dir), 'actionsReplayed': False}))

"""Compatibility entry point; CyCodeCore Manager replaces the old updater UI."""
from pathlib import Path
import runpy
import sys

if __name__ == '__main__':
    if '--page' not in sys.argv:
        sys.argv += ['--page', 'updates']
    runpy.run_path(str(Path(__file__).resolve().parents[1] / 'CyCodeManager/main.py'), run_name='__main__')

"""CyCodeCore Manager: agent skills, project connections and verified updates."""
from pathlib import Path
import ctypes
import os
import sys

HOME = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HOME / 'app'))
from manager_window import run

if __name__ == '__main__':
    try:
        raise SystemExit(run(HOME))
    except Exception as error:
        if sys.stderr:
            print(str(error), file=sys.stderr)
        elif os.name == 'nt':
            ctypes.windll.user32.MessageBoxW(None, str(error), 'CyCodeCore Manager', 0x10)
        raise SystemExit(2)

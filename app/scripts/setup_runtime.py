"""Create an optional stdlib-only Python environment for Manager development."""
from pathlib import Path
import sys
import venv
root = Path(__file__).resolve().parents[2]
directory = root / 'runtime/python'
python = directory / ('Scripts/python.exe' if sys.platform == 'win32' else 'bin/python')
if not python.exists(): venv.create(directory, with_pip=False)
print('Manager Python: ' + str(python))
print('Native CLI/MCP needs no Python. Acceptance-client dependencies: app/requirements-dev.txt (developer only).')

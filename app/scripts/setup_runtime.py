"""Install the optional MCP runtime privately; native editing needs no Python."""
import argparse
from pathlib import Path
import subprocess
import sys
import venv
ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--core-wheel', type=Path, default=ROOT.parent / 'CyToolsCore/dist/cytools_core-0.10.1-py3-none-any.whl')
args = parser.parse_args()
if not args.core_wheel.is_file():
    raise SystemExit('Provide the CyToolsCore 0.10.1 wheel with --core-wheel.')
directory = ROOT / 'runtime/python'
python = directory / ('Scripts/python.exe' if sys.platform == 'win32' else 'bin/python')
if not python.exists():
    venv.create(directory, with_pip=True)
subprocess.run([str(python), '-m', 'pip', 'install', '--keyring-provider', 'disabled', '--disable-pip-version-check',
                str(args.core_wheel.resolve()) + '[mcp]'], check=True)
print(f'Private MCP runtime installed: {python}')

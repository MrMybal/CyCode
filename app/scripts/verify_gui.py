"""Render all CyUI themes plus French/options without changing saved preferences."""
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
executable = ROOT / ('CyCode.exe' if sys.platform == 'win32' else 'CyCode')
report = ROOT / 'data/reports'
report.mkdir(parents=True, exist_ok=True)
cases = [(theme, 'en', False) for theme in ('graphite','navy','mid-grey','light-grey','amber-night','violet')]
cases += [('navy','fr',False), ('light-grey','fr',True)]
for theme, language, options in cases:
    capture = report / f'{theme}-{language}{"-options" if options else ""}.bmp'
    args = [str(executable), '--capture', str(capture), '--theme', theme, '--language', language]
    if options:
        args += ['--show-options', '--capture-panel', 'options']
    startup = None
    if sys.platform == 'win32':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    subprocess.run(args, cwd=ROOT.parent, startupinfo=startup, timeout=25, check=True)
    if not capture.exists() or capture.stat().st_size < 1024:
        raise RuntimeError(f'Missing capture: {capture}')
    print(capture.name)

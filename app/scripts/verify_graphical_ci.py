"""Explicit runner-only smoke checks. Never launches a local desktop session."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('This check is restricted to an explicitly configured GitHub Actions runner.')
    executable = args.executable.resolve(strict=True)
    if not executable.is_relative_to(ROOT / 'runtime'):
        parser.error('Use the production executable from this checkout build.')
    parent = ROOT / 'runtime/test-workspaces/graphical-ci'
    parent.mkdir(parents=True, exist_ok=True)
    reports = ROOT / 'runtime/reports/graphical-ci'
    reports.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='workspace-', dir=parent) as temporary:
        workspace = Path(temporary)
        text = '// Synthetic code example: no user project data.\n#include <string>\nint main() { std::string message = "Hello"; return message.empty(); }\n'
        source = workspace / 'main.cpp'
        source.write_text(text, 'utf-8')
        for language, theme in [('en', 'graphite'), ('fr', 'light-grey')]:
            prefix = reports / (language + '-' + theme)
            capture, report = prefix.with_suffix('.bmp'), prefix.with_suffix('.json')
            command = [str(executable), '--workspace', str(workspace), '--open', 'main.cpp',
                '--language', language, '--theme', theme, '--disable-plugins', '--test-hidden',
                '--test-frames', '90', '--test-wait-idle', '--test-capture', str(capture),
                '--test-report', str(report)]
            result = subprocess.run(command, cwd=workspace, capture_output=True, timeout=90)
            assert result.returncode == 0, result.stderr.decode(errors='replace')
            state = json.loads(report.read_text('utf-8'))
            assert state['hiddenWindow'] and state['active'] == 'main.cpp' and not state['error'], state
            assert state['theme'] == theme and not state['dirty'], state
            assert source.read_text('utf-8') == text, 'Smoke session changed its source file'
            pixels = capture.read_bytes()
            assert pixels[:2] == b'BM' and len(pixels) > 1024
            width, height = struct.unpack_from('<ii', pixels, 18)
            assert width >= 800 and abs(height) >= 500, 'Unexpected capture dimensions'
            offset = struct.unpack_from('<I', pixels, 10)[0]
            assert len(set(pixels[offset:])) > 16, 'Blank or uniform rendered capture'
            print(json.dumps({'language': language, 'theme': theme, 'width': width,
                'height': abs(height), 'captureSha256': hashlib.sha256(pixels).hexdigest()}))
    print('Actual Linux startup/render/source preservation passed; this is not full keyboard/DPI acceptance.')

if __name__ == '__main__':
    main()

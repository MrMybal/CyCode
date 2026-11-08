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
        text = '// Synthetic code example: no user project data.\n// UTF-8: été — Σ — кириллица.\n#include <string>\nint main() { std::string message = "Hello"; return message.empty(); }\n'
        source = workspace / 'main.cpp'
        source.write_bytes(text.encode('utf-8'))
        source_digest=hashlib.sha256(source.read_bytes()).hexdigest()
        theme_hashes=set()
        for language, theme in [('en', theme) for theme in ('graphite','navy','mid-grey','light-grey','amber-night','violet')] + [('fr','graphite'),('fr','light-grey')]:
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
            fonts=state['ui']['fonts']
            assert 'Roboto-Regular.ttf' in fonts['ui'] and 'JetBrainsMono-Regular.ttf' in fonts['mono'], fonts
            assert fonts['portableIconsMerged'] and fonts['icons'], fonts
            assert hashlib.sha256(source.read_bytes()).hexdigest()==source_digest, 'Smoke session changed exact source bytes'
            pixels = capture.read_bytes()
            assert pixels[:2] == b'BM' and len(pixels) > 1024
            width, height = struct.unpack_from('<ii', pixels, 18)
            assert width >= 800 and abs(height) >= 500, 'Unexpected capture dimensions'
            offset = struct.unpack_from('<I', pixels, 10)[0]
            assert len(set(pixels[offset:])) > 16, 'Blank or uniform rendered capture'
            if language=='en':theme_hashes.add(hashlib.sha256(pixels).hexdigest())
            print(json.dumps({'language': language, 'theme': theme, 'width': width,
                'height': abs(height), 'captureSha256': hashlib.sha256(pixels).hexdigest()}))
        assert len(theme_hashes)==6,'Requested English themes did not produce six distinct renders.'
    print('Actual runner startup/render/font/source preservation passed; this is not full keyboard/DPI acceptance.')

if __name__ == '__main__':
    main()

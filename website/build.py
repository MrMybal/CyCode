"""Build the static CyCode showcase and include the unchanged CyDoc export."""
from pathlib import Path
import argparse
import hashlib
import html
import json
import re
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
MARKER = 'showcase-files.json'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inventory(folder):
    result = {}
    for path in folder.rglob('*'):
        if path.is_symlink():
            raise RuntimeError('Preserve linked output paths')
        if path.is_file():
            result[path.relative_to(folder).as_posix()] = digest(path.read_bytes())
    return result


def publish(stage, output):
    if output == ROOT or output == HERE or not output.is_relative_to(ROOT):
        raise RuntimeError('Choose a generated output directory inside this checkout')
    if output.is_symlink():
        raise RuntimeError('Preserve linked output')
    previous = {}
    if output.exists() and any(output.iterdir()):
        marker = output / MARKER
        if not marker.is_file():
            raise RuntimeError('Preserve unrecognised existing output')
        previous = json.loads(marker.read_text('utf-8'))['files']
        actual = inventory(output)
        if set(actual) != set(previous) | {MARKER} or any(actual.get(name) != value for name, value in previous.items()):
            raise RuntimeError('Preserve modified or unknown generated files')
    current = inventory(stage)
    output.mkdir(parents=True, exist_ok=True)
    # Preflight every destination before copying or retiring owned output.
    for name in set(previous) | set(current):
        target = output / name
        if not target.resolve().is_relative_to(output) or target.is_symlink():
            raise RuntimeError('Unsafe generated path')
    for name in current:
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(stage / name, target)
    for name in set(previous) - set(current):
        (output / name).unlink()


def build(output, regenerate=True):
    if regenerate:
        subprocess.run([sys.executable, '-B', str(ROOT / 'app/.CyDocBuild/build.py')], cwd=ROOT, check=True)
    manual = ROOT / 'app/Documentation'
    subprocess.run([sys.executable, '-B', str(ROOT / 'app/scripts/verify_documentation.py'), str(manual)], cwd=ROOT, check=True)
    documentation_files = inventory(manual)
    doc_revision = digest(json.dumps(documentation_files, sort_keys=True).encode())[:16]
    doc_base = f'manual/{doc_revision}/Documentation.html'
    content = json.loads((HERE / 'content.json').read_text('utf-8'))
    assert content and all(set(value) == {'en', 'fr'} and all(isinstance(text, str) and text.strip() for text in value.values()) for value in content.values())
    source = (HERE / 'index.html').read_text('utf-8')
    keys = set(re.findall(r'data-i18n(?:-alt|-aria)?="([^"]+)"', source))
    assert keys <= content.keys(), 'Missing page translations'
    assets = {}
    source_assets = sorted((HERE / 'assets').glob('*')) + [HERE / 'style.css', HERE / 'site.js']
    (ROOT / 'runtime').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='showcase-', dir=ROOT / 'runtime') as temporary:
        stage = Path(temporary)
        for path in source_assets:
            if not path.is_file() or path.is_symlink() or not re.fullmatch(r'[a-z0-9-]+\.(?:png|css|js)', path.name):
                raise RuntimeError('Unexpected showcase asset')
            data = path.read_bytes()
            name = f'assets/{path.stem}.{digest(data)[:16]}{path.suffix}'
            target = stage / name; target.parent.mkdir(exist_ok=True)
            target.write_bytes(data); assets[path.name] = name
        for name, value in assets.items(): source = source.replace('{{asset:' + name + '}}', value)
        source = source.replace('{{description}}', html.escape(content['description']['en'], quote=True))
        source = re.sub(r'\{\{doc:([a-z0-9-]+)\}\}', lambda match:doc_base + '#/en/' + match[1], source)
        # Render a usable English page before JS, including its metadata and links.
        source = re.sub(r'(<([a-z0-9]+)\b[^>]*data-i18n="([^"]+)"[^>]*>).*?(</\2>)',
            lambda match:match[1] + html.escape(content[match[3]]['en']) + match[4], source, flags=re.S)
        source = source.replace('{{content}}', json.dumps(content, ensure_ascii=False).replace('</', '<\\/'))
        source = source.replace('{{config}}', json.dumps({'documentation':doc_base, 'assets':assets}, ensure_ascii=False).replace('</', '<\\/'))
        assert not re.search(r'\{\{(?:asset:|doc:|content|config|description)', source), 'Unresolved template token'
        (stage / 'index.html').write_text(source, 'utf-8')
        # Stable public entry point; the actual, unchanged reader keeps its
        # content-versioned resource URLs and the requested language/section.
        redirect = ('<!doctype html><html lang="en"><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1">'
            '<title>CyCode documentation</title><script>location.replace('
            + json.dumps(doc_base) + ' + location.search + (location.hash || "#/en/overview"));</script>'
            '<body><p><a href="' + doc_base + '#/en/overview">English documentation</a> · '
            '<a href="' + doc_base + '#/fr/overview">Documentation française</a></p></body></html>')
        (stage / 'Documentation.html').write_text(redirect, 'utf-8')
        (stage / '.nojekyll').write_text('', 'utf-8')
        shutil.copyfile(ROOT / 'LICENSE', stage / 'LICENSE.txt')
        shutil.copytree(manual, stage / 'manual' / doc_revision)
        # The official reader is copied byte-for-byte, with a versioned directory
        # so every resource URL changes when any part of the export changes.
        assert inventory(stage / 'manual' / doc_revision) == documentation_files
        (stage / MARKER).write_text(json.dumps({'format':1, 'files':inventory(stage)}, sort_keys=True, indent=2)+'\n','utf-8')
        publish(stage, output)
    print(f'Local showcase: {output / "index.html"}')
    print(f'CyDoc export retained unchanged at {doc_base}; scripts, styles and images have content-hashed URLs.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'runtime/showcase')
    parser.add_argument('--skip-docs-build', action='store_true', help='Reuse an already generated and verified local manual')
    args = parser.parse_args()
    build(args.output.resolve(), not args.skip_docs_build)

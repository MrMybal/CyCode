"""Build the local, bilingual user manual with the locked official CyDoc package."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from prepare import prepare
from api import generate
from verify import verify

WORK = Path(__file__).resolve().parent
APP = WORK.parent
MARKER = 'DocData/export-files.json'


def inventory(directory):
    result = {}
    for path in directory.rglob('*'):
        if path.is_symlink(): raise RuntimeError('Preserve linked documentation paths: ' + str(path))
        if path.is_file(): result[path.relative_to(directory).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def publish(source, destination):
    """Replace only unchanged, owned output; never erase arbitrary local files."""
    source, destination = source.resolve(), destination.resolve()
    if source == destination: return
    files = inventory(source)
    previous = {}
    marker = MARKER
    if destination.exists() and any(destination.iterdir()):
        if not (destination / marker).is_file(): marker = 'cydoc-export.json'
        if not (destination / marker).is_file(): raise RuntimeError('Preserve unrecognized output directory: ' + str(destination))
        previous = json.loads((destination / marker).read_text('utf-8'))['files']
        actual = inventory(destination)
        if set(actual) != set(previous) | {marker}: raise RuntimeError('Preserve unknown or missing output files: ' + str(destination))
        if any(actual[name] != digest for name, digest in previous.items()): raise RuntimeError('Preserve local changes in output: ' + str(destination))
    destination.mkdir(parents=True, exist_ok=True)
    for name in files:
        target = destination / name
        if not target.resolve().is_relative_to(destination) or target.is_symlink(): raise RuntimeError('Unsafe export path')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, target)
    for name in (set(previous) | {marker}) - set(files):
        target = destination / name
        if not target.resolve().is_relative_to(destination) or target.is_symlink(): raise RuntimeError('Unsafe previous export path')
        target.unlink(missing_ok=True)
    # A flat-to-DocData migration also retires directories that contained only
    # old owned resources. Remove exact empty parents, never recurse over data.
    parents = {parent for name in previous for parent in (destination / name).parents if parent != destination and parent.is_relative_to(destination)}
    for parent in sorted(parents, key=lambda path: len(path.parts), reverse=True):
        if parent.is_symlink(): raise RuntimeError('Unsafe previous export directory')
        if parent.is_dir() and not any(parent.iterdir()): parent.rmdir()


def build(outputs=()):
    node, package = prepare()
    settings = json.loads((WORK / 'cydoc.config.json').read_text('utf-8'))
    descriptor = json.loads((APP / 'CyTool.json').read_text('utf-8'))
    if settings['version'] != descriptor['version']: raise RuntimeError('Update the documentation version with the application descriptor')
    generate()
    subprocess.run([node, str(WORK / 'generate_api.mjs')], check=True)
    # The official CLI's `check` also writes an export. Always give it a fresh,
    # private application-shaped staging directory, never the installed manual.
    with tempfile.TemporaryDirectory(prefix='export-', dir=WORK / '.cache') as temporary:
        stage = Path(temporary)
        config_dir = stage / '.CyDocBuild'; config_dir.mkdir()
        config = dict(settings, content=os.path.relpath(WORK / 'manual', config_dir).replace('\\', '/'), assets=os.path.relpath(WORK / 'assets', config_dir).replace('\\', '/'))
        (config_dir / 'cydoc.config.json').write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding='utf-8')
        site = stage / 'Documentation'; site.mkdir()
        legacy = WORK / 'internal/legacy-index.html'
        if legacy.is_file(): shutil.copyfile(legacy, site / 'index.html')
        subprocess.run([node, str(package / 'tools/cli.mjs'), 'check', str(config_dir / 'cydoc.config.json')], check=True)
        licenses = site / 'DocData/licenses'; licenses.mkdir()
        for filename, source in {'CyDoc.txt': package / 'LICENSE', 'Marked.txt': WORK / 'node_modules/marked/LICENSE.md', 'PrismJS.txt': WORK / 'node_modules/prismjs/LICENSE', 'esbuild.txt': WORK / 'node_modules/esbuild/LICENSE.md'}.items():
            shutil.copyfile(source, licenses / filename)
        with (site / 'DocAI.md').open('a', encoding='utf-8') as ai:
            ai.write('\n## CyCode integration\n\nUse the official CyToolsCore SDK/MCP companion and native services. Editing requires explicit permission. Read document revisions before changes, preview transactions, and save only when requested. Poll asynchronous jobs and inspect process exit codes. Never infer language-service or debugger capabilities from syntax colouring. The public operation schemas are in DocData/manifest.json; implementation sources, local project data and credentials are not distributed.\n')
        (site / MARKER).write_text(json.dumps({'format': 2, 'generator': 'CyDoc', 'revision': json.loads((WORK / 'cydoc.lock.json').read_text('utf-8'))['revision'], 'files': inventory(site)}, indent=2) + '\n', encoding='utf-8')
        verify(site)
        for destination in [APP / 'Documentation', *outputs]: publish(site, destination)
    print('CyDoc manual: ' + str(APP / 'Documentation/Documentation.html'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, action='append', default=[])
    args = parser.parse_args()
    build(args.output)


if __name__ == '__main__': main()

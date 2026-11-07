"""Check the Pages payload, translations, links and unchanged offline manual."""
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlsplit, unquote
import hashlib
import json
import re
import struct
import sys

HERE = Path(__file__).resolve().parent


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.nodes = []
        self.ids = set()
        self.scripts = {}
        self.script_id = None

    def handle_starttag(self, tag, attributes):
        attrs = dict(attributes)
        self.nodes.append((tag, attrs))
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, f'Duplicate HTML id: {attrs["id"]}'
            self.ids.add(attrs['id'])
        if tag == 'script' and attrs.get('type') == 'application/json':
            self.script_id = attrs['id']
            self.scripts[self.script_id] = ''

    def handle_data(self, data):
        if self.script_id:
            self.scripts[self.script_id] += data

    def handle_endtag(self, tag):
        if tag == 'script':
            self.script_id = None


def verify(output):
    output = output.resolve()
    marker = json.loads((output / 'showcase-files.json').read_text('utf-8'))
    assert marker['format'] == 1
    files = {}
    for path in output.rglob('*'):
        assert not path.is_symlink(), f'Linked output: {path.name}'
        if path.is_file() and path.name != 'showcase-files.json':
            name = path.relative_to(output).as_posix()
            assert name in ('.nojekyll', 'index.html', 'Documentation.html', 'LICENSE.txt') or name.startswith(('assets/', 'manual/')), f'Unexpected published file: {name}'
            assert not {'.git', '.CyDocBuild', 'runtime', 'data', 'node_modules'} & set(path.relative_to(output).parts), f'Private build material: {name}'
            files[name] = sha(path.read_bytes())
    assert files == marker['files'], 'Output ownership or file hashes differ'
    assert (output / '.nojekyll').is_file()
    assert (output / 'LICENSE.txt').read_bytes() == (HERE.parent / 'LICENSE').read_bytes()
    page = Page()
    source = (output / 'index.html').read_text('utf-8')
    page.feed(source)
    assert '{{asset:' not in source and '{{doc:' not in source
    copy = json.loads(page.scripts['site-content'])
    config = json.loads(page.scripts['site-config'])
    assert copy == json.loads((HERE / 'content.json').read_text('utf-8'))
    assert all(set(value) == {'en', 'fr'} and all(isinstance(text, str) and text.strip() for text in value.values()) for value in copy.values()), 'Incomplete bilingual content'
    manual = output / config['documentation']
    assert manual.name == 'Documentation.html' and manual.is_file()
    manual_files = {path.relative_to(manual.parent).as_posix(): sha(path.read_bytes()) for path in manual.parent.rglob('*') if path.is_file()}
    assert manual.parent.name == sha(json.dumps(manual_files, sort_keys=True).encode())[:16], 'CyDoc version directory differs'
    original_manual = HERE.parent / 'app/Documentation'
    original_files = {path.relative_to(original_manual).as_posix(): sha(path.read_bytes()) for path in original_manual.rglob('*') if path.is_file()}
    assert manual_files == original_files, 'The official local CyDoc export was altered or omitted'
    redirect = Page()
    redirect.feed((output / 'Documentation.html').read_text('utf-8'))
    assert {attrs['href'] for tag, attrs in redirect.nodes if tag == 'a'} == {
        config['documentation'] + '#/en/overview', config['documentation'] + '#/fr/overview'}
    manifest = json.loads((manual.parent / 'DocData/manifest.json').read_text('utf-8'))
    pages = {item['id'] for item in manifest['pages']}
    assert manifest['defaultLocale'] == 'en' and {'en', 'fr'} <= {item['code'] for item in manifest['locales']}
    links = images = 0
    for tag, attrs in page.nodes:
        for attr in ('data-i18n', 'data-i18n-alt', 'data-i18n-aria', 'data-zoom', 'data-gallery-caption'):
            if attr in attrs:
                assert attrs[attr] in copy, f'Missing translation: {attrs[attr]}'
        if 'data-doc' in attrs:
            assert attrs['data-doc'] in pages, f'Unknown manual page: {attrs["data-doc"]}'
            assert attrs.get('href') == config['documentation'] + '#/en/' + attrs['data-doc']
        if 'data-gallery-file' in attrs:
            assert attrs['data-gallery-file'] in config['assets']
        if tag == 'img':
            images += 1
            assert 'alt' in attrs
            assert attrs.get('src') or attrs == {'alt': ''}, 'Missing image source'
        for attr in ('src', 'href'):
            if attr not in attrs:
                continue
            link = urlsplit(attrs[attr])
            assert link.scheme in ('', 'https'), 'Unexpected URL protocol'
            if link.scheme:
                assert tag == 'a', 'External runtime dependency'
                continue
            assert not link.netloc and not link.path.startswith('/'), 'Not portable under a Pages project path'
            if link.path:
                target = (output / unquote(link.path)).resolve()
                assert target.is_relative_to(output) and target.is_file(), f'Broken local link: {attrs[attr]}'
            elif link.fragment:
                assert link.fragment in page.ids, f'Unknown section: {link.fragment}'
            links += 1
    for original, name in config['assets'].items():
        path = output / name
        data = path.read_bytes()
        assert re.fullmatch(r'assets/[a-z0-9-]+\.' + sha(data)[:16] + r'\.(png|js|css)', name), f'Unversioned asset: {name}'
        if path.suffix == '.png':
            assert data[:8] == b'\x89PNG\r\n\x1a\n' and data[12:16] == b'IHDR'
            width, height = struct.unpack('>II', data[16:24])
            assert width > 0 and height > 0 and width * height < 40_000_000, f'Invalid image: {original}'
    print(f'Verified {len(copy)} EN/FR strings, {links} local references, {images} image elements, {len(config["assets"])} versioned assets and {len(manual_files)} unchanged CyDoc files.')


if __name__ == '__main__':
    verify(Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent / 'runtime/showcase')

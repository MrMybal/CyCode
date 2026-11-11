"""Verify the distributed manual; no native executable or server is started."""
import argparse
import hashlib
import json
import re
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


class References(HTMLParser):
    def __init__(self, html):
        super().__init__(); self.links = []; self.assets = []; self.ids = set(); self.headings = set()
        self.feed(html)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs: self.ids.add(attrs['id'])
        if tag in {'h1','h2','h3','h4','h5','h6'} and 'id' in attrs: self.headings.add(attrs['id'])
        if tag == 'a' and 'href' in attrs: self.links.append(attrs['href'])
        for name in ('src','poster','data-src'):
            if name in attrs: self.assets.append(attrs[name])
        if tag == 'link' and 'href' in attrs: self.assets.append(attrs['href'])


def verify_source_coverage(app=None):
    """Check reviewed inputs without changing their hashes or preparing an export."""
    app = Path(app) if app is not None else Path(__file__).resolve().parent.parent
    coverage = json.loads((app / '.CyDocBuild/coverage.json').read_text('utf-8'))
    assert coverage['format'] == 1
    entries = coverage['sources']
    indexed = {entry['source'] for entry in entries}
    assert len(indexed) == len(entries), 'Duplicate documentation source'
    assert indexed == {p.relative_to(app).as_posix() for p in (app / 'Docs').rglob('*.md')}, 'Review documentation source coverage'
    for entry in entries:
        assert entry['kind'] in {'developer-reference', 'user-workflow'}
        assert re.fullmatch('[0-9a-f]{64}', entry['sourceSha256'])
        digest = hashlib.sha256((app / entry['source']).read_text('utf-8').encode('utf-8')).hexdigest()
        assert digest == entry['sourceSha256'], 'Review changed documentation source: ' + entry['source']
    return entries


def verify_native_help(parsed, ui_sources=None, app=None):
    app = Path(app) if app is not None else Path(__file__).resolve().parent.parent
    routes = re.findall(r'\{"([a-zA-Z]+)", "([a-z0-9-]+)", "([a-z0-9-]+)"\}', (app / 'src/HelpTopics.h').read_text('utf-8'))
    assert routes and len({route[0] for route in routes}) == len(routes), 'Invalid native help catalogue'
    panels, literal_routes = set(), set()
    for source in (ui_sources or app / 'src').glob('*.cpp'):
        text = source.read_text('utf-8')
        panels.update(re.findall(r'beginPanel\([^,\n]+,\s*"([a-zA-Z]+)"\)', text))
        literal_routes.update(re.findall(
            r'app_help::open\([^;\n]*?"app/Documentation"\s*,\s*[^,\n]+,\s*"([a-z0-9-]+)"\s*,\s*"([a-z0-9-]*)"', text))
    assert panels <= {route[0] for route in routes}, 'A built-in panel has no contextual help: ' + ', '.join(sorted(panels - {route[0] for route in routes}))
    targets = {(identity, anchor) for _, identity, anchor in routes} | literal_routes
    for identity, anchor in targets:
        for locale in ('en', 'fr'):
            assert (locale, identity) in parsed and (not anchor or anchor in parsed[locale, identity].ids), 'Broken native context help: ' + locale + '/' + identity + '#' + anchor
    return len(routes), len(literal_routes)


def verify(site, ui_sources=None):
    entries = verify_source_coverage()
    site = site.resolve(); data = site / 'DocData'
    manifest = json.loads((data / 'manifest.json').read_text('utf-8'))
    owned = json.loads((data / 'export-files.json').read_text('utf-8'))['files']
    actual = {p.relative_to(site).as_posix() for p in site.rglob('*') if p.is_file()}
    assert actual == set(owned) | {'DocData/export-files.json'}, 'Unexpected or missing output files'
    assert {p.name for p in site.iterdir()} <= {'Documentation.html', 'DocAI.md', 'DocData', 'index.html'}
    assert (site / 'DocAI.md').is_file()
    def local(url, base=data):
        parts = urlsplit(url)
        assert not parts.scheme and not parts.netloc, 'Nonlocal runtime asset: ' + url
        target = (base / unquote(parts.path)).resolve()
        assert target.is_relative_to(site) and target.is_file() and not target.is_symlink(), 'Missing/unsafe asset: ' + url
    for name, digest in owned.items():
        local(name, site)
        assert hashlib.sha256((site / name).read_bytes()).hexdigest() == digest, 'Changed output: ' + name
        assert (site / name).stat().st_size < 64 * 1024 * 1024
        assert not any(part in {'.CyDocBuild','internal','node_modules','runtime','SourceFile'} for part in Path(name).parts)
    assert {p.name for p in data.glob('media-*.js')} == {'media-image.js', 'media-gallery.js'}
    assert not list(site.rglob('*three*'))
    shell = References((site / 'Documentation.html').read_text('utf-8'))
    for asset in shell.assets: local(asset, site)
    assert manifest['defaultLocale'] == 'en'
    assert {entry['code']: entry['label'] for entry in manifest['locales']} == {'en':'English','fr':'Français'}
    pages = {(p['locale'], p['id']): p for p in manifest['pages']}
    assert len(pages) == len(manifest['pages']), 'Duplicate page identity'
    ids = {identity for locale, identity in pages if locale == 'en'}
    assert ids == {identity for locale, identity in pages if locale == 'fr'}, 'Incomplete language'
    parsed = {key: References(page['html']) for key, page in pages.items()}
    aliases = manifest.get('aliases', {})
    def resolve(identity, anchor=''):
        seen = set()
        while identity in aliases:
            assert identity not in seen; seen.add(identity)
            value = aliases[identity].split('#', 1)
            identity = value[0]; anchor = anchor or (value[1] if len(value) > 1 else '')
        return identity, anchor
    for alias in aliases:
        identity, anchor = resolve(alias)
        assert identity in ids, 'Broken alias: ' + alias
        if anchor: assert anchor in parsed['en', identity].ids and anchor in parsed['fr', identity].ids
    checks = 0
    for key, page in pages.items():
        assert page.get('kind') != 'source' and not page.get('sourcePageId') and not page.get('source')
        assert not page['id'].startswith(('reference-', 'source-')) and page['id'] not in {'translation-report', 'help-routing'}
        if page.get('parent'): assert (key[0], page['parent']) in pages
        for asset in page.get('assets', []): local(asset)
        for asset in parsed[key].assets: local(asset)
        for link in parsed[key].links:
            if not link.startswith('#/'): continue
            route = [unquote(value) for value in link[2:].split('?')[0].split('/')]
            identity, anchor = resolve(route[1], route[2] if len(route) > 2 else '')
            target = parsed.get((route[0], identity))
            assert target, str(key) + ': missing page ' + link
            if anchor: assert anchor in target.ids, str(key) + ': missing anchor ' + link
            checks += 1
    for identity in ids:
        assert parsed['en', identity].headings == parsed['fr', identity].headings, 'Translated anchors differ: ' + identity
        assert pages['en', identity]['html'] != pages['fr', identity]['html'], 'Untranslated page: ' + identity
    # Check source-guide coverage and the actual native help addresses. These
    # inputs are maintenance data only; no source/coverage file is exported.
    app = Path(__file__).resolve().parent.parent
    for entry in entries:
        assert aliases[entry['legacyId']] == entry['target'], 'Source guide lost its legacy route'
        identity, _, anchor = entry['target'].partition('#')
        for locale in ('en', 'fr'):
            assert (locale, identity) in parsed and anchor in parsed[locale, identity].ids
    topic_count, literal_count = verify_native_help(parsed, ui_sources)
    report = json.loads((data / 'translation-report.json').read_text('utf-8'))
    assert not report.get('missing') and not report.get('outdated'), 'Incomplete translations'
    descriptor = Path(__file__).resolve().parents[1] / 'CyTool.json'
    operations = {op['id'] for op in json.loads(descriptor.read_text('utf-8'))['operations']}
    assert {identity for identity in ids if identity.startswith('operation-')} == {'operation-' + op.replace('_','-') + '-' + op.replace('_','-') for op in operations}
    assert sum(bool(p.get('parent')) for p in pages.values()) > len(pages) * .9, 'Missing hierarchy'
    for notice in ('CyDoc.txt','Marked.txt','PrismJS.txt','esbuild.txt'): assert (data / 'licenses' / notice).stat().st_size > 100
    print(f'{manifest["name"]}: {len(pages)} localised pages; {checks} routes; {len(entries)} source guides; {topic_count} native help topics; {literal_count} explicit help routes; EN/FR complete; image/gallery; public API only.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('site', type=Path, nargs='?')
    parser.add_argument('--sources-only', action='store_true', help='Check reviewed source coverage without Node, export or native processes')
    parser.add_argument('--ide-root', type=Path, help='Check context help against the extracted IDE sources')
    args = parser.parse_args()
    if args.sources_only:
        print(f'{len(verify_source_coverage())} reviewed source guides: coverage current.')
    elif args.site:
        verify(args.site, ui_sources=args.ide_root / 'app/src' if args.ide_root else None)
    else:
        parser.error('provide a site or --sources-only')

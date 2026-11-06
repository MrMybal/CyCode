"""Validate a complete exported manual without launching an application/browser."""
import argparse
import hashlib
import json
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

class References(HTMLParser):
    def __init__(self, html):
        super().__init__(); self.links=[]; self.assets=[]; self.ids=set(); self.headings=set()
        self.feed(html)
    def handle_starttag(self, tag, attrs):
        attrs=dict(attrs)
        if 'id' in attrs: self.ids.add(attrs['id'])
        if tag in {'h1','h2','h3','h4','h5','h6'} and 'id' in attrs: self.headings.add(attrs['id'])
        if tag=='a' and 'href' in attrs: self.links.append(attrs['href'])
        for name in ('src','poster','data-src'):
            if name in attrs: self.assets.append(attrs[name])
        if tag=='link' and 'href' in attrs: self.assets.append(attrs['href'])

def verify(site):
    site=site.resolve()
    manifest=json.loads((site/'manifest.json').read_text(encoding='utf-8'))
    migration=json.loads((site/'migration-report.json').read_text(encoding='utf-8'))
    owned=json.loads((site/'cydoc-export.json').read_text(encoding='utf-8'))['files']
    actual={p.relative_to(site).as_posix() for p in site.rglob('*') if p.is_file()}
    assert actual==set(owned)|{'cydoc-export.json'}, 'Unexpected or missing export files'
    def local(url):
        parts=urlsplit(url)
        assert not parts.scheme and not parts.netloc, f'Nonlocal runtime asset: {url}'
        path=(site/unquote(parts.path)).resolve()
        assert path.is_relative_to(site) and path.is_file(), f'Missing/unsafe asset: {url}'
    for name,digest in owned.items():
        local(name)
        assert hashlib.sha256((site/name).read_bytes()).hexdigest()==digest, f'Changed export: {name}'
        assert (site/name).stat().st_size < 64*1024*1024, f'Updater file limit: {name}'
    assert sum((site/name).stat().st_size for name in owned)<256*1024*1024
    assert migration['modules']==['image','gallery']
    assert {p.name for p in site.glob('media-*.js')}=={'media-image.js','media-gallery.js'}
    assert not list(site.rglob('*three*'))
    shell=References((site/'index.html').read_text(encoding='utf-8'))
    for ref in shell.assets: local(ref)
    assert manifest['defaultLocale']=='en'
    assert {locale['code'] for locale in manifest['locales']}=={'en','fr'}
    pages={(page['locale'],page['id']):page for page in manifest['pages']}
    parsed={key:References(page['html']) for key,page in pages.items()}
    aliases=manifest.get('aliases',{})
    checks=0
    for key,page in pages.items():
        for asset in page.get('assets',[]): local(asset)
        for asset in parsed[key].assets: local(asset)
        for link in parsed[key].links:
            if not link.startswith('#/'): continue
            route=[unquote(value) for value in link[2:].split('?')[0].split('/')]
            locale,identity=route[:2];identity=aliases.get(identity,identity)
            target=parsed.get((locale,identity)) or parsed.get(('en',identity))
            assert target is not None, f'{key}: missing page {link}'
            if len(route)>2 and route[2]: assert route[2] in target.ids, f'{key}: missing section {link}'
            checks+=1
    guide_ids=['overview','quickstart','tour','editing','search','shortcuts','languages',
               'projects','plugins','version-control','mcp','updates','limits']
    for identity in guide_ids:
        assert parsed['en',identity].ids==parsed['fr',identity].ids, f'Translated anchors differ: {identity}'
    for locale,identity in parsed:
        if locale=='fr' and ('en',identity) in parsed:
            assert parsed['en',identity].headings<=parsed['fr',identity].headings, f'Missing translated section aliases: {identity}'
    for locale in ('en','fr'):
        assert 'interface' in parsed[locale,'tour'].ids
        assert 'interface-image' in pages[locale,'tour']['html']
        assert 'documents' in parsed[locale,'editing'].ids
    assert all(('en',identity) in pages for identity in migration['pageMap'].values())
    assert len([page for page in manifest['pages'] if page['id'].startswith('operation-')])==migration['apiOperations']
    assert not migration['legacyMissingLinks'], 'Review unresolved legacy references'
    assert (site/'translation-report.json').is_file()
    for notice in ['CyDoc.txt','Marked.txt','PrismJS.txt','esbuild.txt']:
        assert (site/'licenses'/notice).stat().st_size>100
    print(f"{manifest['name']}: {len(pages)} localized pages; {checks} local routes; "
          f"{len(owned)} owned files; image/gallery only; all checks passed.")

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('site',type=Path)
    verify(parser.parse_args().site)

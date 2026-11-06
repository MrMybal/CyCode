"""Exercise output ownership, archive completeness and native help destinations."""
import argparse
import hashlib
import json
import re
import tempfile
import zipfile
from pathlib import Path
from build import inventory, publish
from verify import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('site', type=Path)
    args = parser.parse_args()
    site = args.site.resolve(); verify(site)
    work = Path(__file__).resolve().parent
    pages = json.loads((site / 'DocData/manifest.json').read_text('utf-8'))['pages']
    identities = {page['id'] for page in pages}
    header = (work.parent / 'src/ApplicationHelp.h').read_text('utf-8')
    topics = re.findall(r'\{"[^"\n]+", "([^"\n]+)"\}', header)
    assert topics and set(topics) <= identities, 'Native panel help must resolve'
    for source in ('Application.cpp','CommandUI.cpp'):
        text = (work.parent / 'src' / source).read_text('utf-8')
        assert '"app/Documentation"' in text and '"app/help"' not in text
    assert 'if (suppressed) return;' in header and '"Documentation.html"' in header
    with tempfile.TemporaryDirectory(prefix='distribution-', dir=work / '.cache') as directory:
        temporary = Path(directory); copied = temporary / 'owned'
        legacy = temporary / 'flat'
        (legacy / 'assets').mkdir(parents=True)
        old = legacy / 'assets/old.txt'; old.write_text('previous owned resource', encoding='utf-8')
        (legacy / 'cydoc-export.json').write_text(json.dumps({'files': {'assets/old.txt': hashlib.sha256(old.read_bytes()).hexdigest()}}), encoding='utf-8')
        publish(site, legacy)
        assert inventory(site) == inventory(legacy)
        assert not (legacy / 'assets').exists(), 'Flat export must retire empty owned resource folders'
        publish(site, copied)
        assert inventory(site) == inventory(copied)
        publish(site, copied)
        unknown = copied / 'user-note.txt'; unknown.write_text('keep me', encoding='utf-8')
        try: publish(site, copied)
        except RuntimeError: pass
        else: raise AssertionError('Unknown destination files must block replacement')
        assert unknown.read_text('utf-8') == 'keep me'; unknown.unlink()
        changed = copied / 'DocAI.md'; changed.write_text('local edit', encoding='utf-8')
        try: publish(site, copied)
        except RuntimeError: pass
        else: raise AssertionError('Local output edits must block replacement')
        assert changed.read_text('utf-8') == 'local edit'
        archive = temporary / 'manual.zip'
        expected = inventory(site)
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as stream:
            for name in expected: stream.write(site / name, 'app/Documentation/' + name)
        with zipfile.ZipFile(archive) as stream:
            assert set(stream.namelist()) == {'app/Documentation/' + name for name in expected}
            for name, digest in expected.items():
                assert hashlib.sha256(stream.read('app/Documentation/' + name)).hexdigest() == digest
            assert not any('.CyDocBuild' in name or '/internal/' in name for name in stream.namelist())
    print(f'Ownership conflict protection, complete archive and {len(topics)} native help destinations passed. No application launched.')


if __name__ == '__main__': main()

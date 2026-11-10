"""Data/HTTP-contract tests for Core updates; no installer or Core is executed."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import core_updates as updates


def release(number='0.1.2', payload=b'synthetic installer contract data'):
    name = 'CyCodeCore-' + number + '-windows-x64-setup.exe'
    digest = hashlib.sha256(payload).hexdigest()
    return {'tag_name': 'core-v' + number, 'draft': False, 'prerelease': True,
            'html_url': 'https://github.com/MrMybal/CyCode/releases/tag/core-v' + number,
            'body': 'Release description', 'assets': [
                {'id': 12, 'name': name, 'url': updates.API + '/releases/assets/12',
                 'size': len(payload), 'digest': 'sha256:' + digest},
                {'id': 13, 'name': 'SHA256SUMS.txt', 'url': updates.API + '/releases/assets/13', 'size': 200}]}


class Client:
    def __init__(self, rows, payload=b'synthetic installer contract data'):
        self.rows = rows
        self.payload = payload
        self.calls = 0

    def releases(self):
        self.calls += 1
        return self.rows

    def read(self, url, maximum, binary=False):
        row = self.rows[0]['assets'][0]
        return (row['digest'][7:] + '  ' + row['name'] + '\n').encode()

    def open(self, url, binary=False):
        return io.BytesIO(self.payload)


class CoreUpdateTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='cycode-update-data-')
        self.home = Path(self.directory.name) / 'home'
        self.cache = Path(self.directory.name) / 'cache'
        self.home.mkdir()
        (self.home / 'core-distribution.json').write_text(json.dumps({
            'product': 'CyCodeCore', 'state': 'complete', 'version': '0.1.1', 'channel': 'preview'}), encoding='utf-8')

    def tearDown(self):
        self.directory.cleanup()

    def test_numeric_version_order(self):
        chosen = updates.select_release([release('0.1.9'), release('0.1.10')], '0.1.1')
        self.assertEqual(chosen['version'], '0.1.10')

    def test_other_products_drafts_and_older_releases_are_ignored(self):
        other = release('0.2.3'); other['tag_name'] = 'v0.2.3'
        draft = release(); draft['draft'] = True
        self.assertIsNone(updates.select_release([other, draft, release('0.1.0')], '0.1.1'))

    def test_preview_requires_preview_channel(self):
        self.assertIsNone(updates.select_release([release()], '0.1.1', 'stable'))
        self.assertIsNotNone(updates.select_release([release()], '0.1.1', 'preview'))

    def test_asset_origin_and_duplicate_checksum_are_rejected(self):
        row = release(); row['assets'][0]['url'] = 'https://example.invalid/setup'
        self.assertIsNone(updates.select_release([row], '0.1.1'))
        row = release(); row['assets'].append(dict(row['assets'][1]))
        self.assertIsNone(updates.select_release([row], '0.1.1'))

    def test_checksum_requires_one_exact_basename(self):
        line = 'a' * 64 + '  setup.exe\n'
        self.assertEqual(updates.checksum(line.encode(), 'setup.exe'), 'a' * 64)
        with self.assertRaises(updates.UpdateError):
            updates.checksum((line + line).encode(), 'setup.exe')
        with self.assertRaises(updates.UpdateError):
            updates.checksum(('a' * 64 + '  ../setup.exe').encode(), 'setup.exe')

    def test_check_cache_and_explicit_refresh(self):
        client = Client([release()])
        first = updates.check(self.home, cache=self.cache, client=client, now=100000)
        self.assertEqual(first['status'], 'available')
        updates.check(self.home, cache=self.cache, client=client, now=100001)
        self.assertEqual(client.calls, 1)
        updates.check(self.home, cache=self.cache, client=client, now=100002, force=True)
        self.assertEqual(client.calls, 2)

    def test_agents_only_installation_is_not_updatable(self):
        path = self.home / 'core-distribution.json'
        data = json.loads(path.read_text('utf-8')); data['state'] = 'agents-only'
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaises(updates.UpdateError):
            updates.check(self.home, cache=self.cache, client=Client([]))

    def test_changed_offer_does_not_start_download(self):
        offered = updates.select_release([release()], '0.1.1')
        with self.assertRaises(updates.UpdateError):
            updates.download(self.home, offered, cache=self.cache, client=Client([release('0.1.3')]))
        self.assertFalse((self.cache / 'downloads').exists())

    def test_bad_payload_is_not_published_as_an_executable(self):
        offered = updates.select_release([release()], '0.1.1')
        with self.assertRaises(updates.UpdateError):
            updates.download(self.home, offered, cache=self.cache, client=Client([release()], b'wrong'))
        self.assertEqual(list((self.cache / 'downloads').iterdir()), [])

    def test_redirect_never_transmits_credential_to_asset_host(self):
        handler = updates.SafeRedirect()
        request = urllib.request.Request(updates.API + '/releases/assets/12',
                                         headers={'Authorization': 'Bearer synthetic-credential'})
        redirected = handler.redirect_request(request, None, 302, '', {},
                                               'https://release-assets.githubusercontent.com/example')
        self.assertIsNone(redirected.get_header('Authorization'))
        with self.assertRaises(updates.UpdateError):
            handler.redirect_request(request, None, 302, '', {}, 'https://example.invalid/file')


if __name__ == '__main__':
    unittest.main()

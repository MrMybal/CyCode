"""Update orchestration and outcome checks; installer invocation is a data stub."""
from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import core_updates as updates
import core_installation
import core_agents


class UpdateOutcomes(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.home = self.root / 'flat'
        self.home.mkdir()
        self.data = {'product': 'CyCodeCore', 'state': 'complete', 'version': '0.1.3', 'channel': 'preview'}
        (self.home / 'core-distribution.json').write_text(json.dumps(self.data))
        self.offer = {'version': '0.1.4'}
        self.setup = self.root / 'installer.contract-data'
        self.setup.write_bytes(b'not an executable')
        self.downloader = patch.object(updates, 'download', return_value=self.setup).start()
        self.addCleanup(patch.stopall)

    def test_success_requires_activated_and_verified_new_payload(self):
        new = self.home / 'versions/0.1.4-new'
        manager = Mock()
        manager.apply.return_value = {'changes': ['connection'], 'skipped': []}
        runner = Mock(return_value=0)
        with patch.object(core_installation, 'current_payload', return_value=new), \
             patch.object(core_installation, 'verify_payload') as verify, \
             patch.object(core_agents, 'Manager', return_value=manager):
            result = updates.install_update(self.home, self.offer, runner=runner, language='fr')
        self.assertEqual(result['status'], 'installed')
        self.assertEqual(result['home'], str(new))
        verify.assert_called_once_with(new, '0.1.4')
        arguments = runner.call_args.args[0]
        self.assertIn('/DIR=' + str(self.home), arguments)
        self.assertIn('/MANAGERUPDATE=1', arguments)
        self.assertNotIn('/VERYSILENT', arguments)
        self.assertTrue(result['reloadClientsRequired'])

    def test_cancel_or_installer_error_never_reports_installation_success(self):
        runner = Mock(return_value=2)
        with self.assertRaises(updates.UpdateError):
            updates.install_update(self.home, self.offer, runner=runner)
        runner.reset_mock()
        with self.assertRaises(updates.UpdateError):
            updates.install_update(self.home, self.offer, runner=runner, cancelled=lambda: True)
        runner.assert_not_called()

    def test_exit_zero_without_new_activation_is_not_success(self):
        with patch.object(core_installation, 'current_payload', return_value=self.home), \
             patch.object(core_installation, 'verify_payload'):
            with self.assertRaisesRegex(updates.UpdateError, 'did not select'):
                updates.install_update(self.home, self.offer, runner=lambda argv: 0)

    def test_connection_write_conflict_is_reported_separately_from_verified_installation(self):
        new = self.home / 'versions/new'
        manager = Mock()
        manager.apply.side_effect = ValueError('settings changed after review')
        with patch.object(core_installation, 'current_payload', return_value=new), \
             patch.object(core_installation, 'verify_payload'), \
             patch.object(core_agents, 'Manager', return_value=manager):
            result = updates.install_update(self.home, self.offer, runner=lambda argv: 0)
        self.assertEqual(result['status'], 'installed')
        self.assertIn('settings changed', result['connections']['skipped'][0])

    def test_equal_versions_in_different_installations_do_not_share_a_cached_offer(self):
        other = self.root / 'other'
        other.mkdir()
        (other / 'core-distribution.json').write_text(json.dumps(self.data))
        client = Mock()
        client.releases.return_value = []
        cache = self.root / 'cache'
        first = updates.check(self.home, cache=cache, client=client, now=100000)
        second = updates.check(other, cache=cache, client=client, now=100001)
        self.assertEqual(client.releases.call_count, 2)
        self.assertNotEqual(first['installationId'], second['installationId'])


if __name__ == '__main__':
    unittest.main()

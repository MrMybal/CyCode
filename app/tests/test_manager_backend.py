"""Reviewed UI actions on temporary settings; no clients/installers launch."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import core_agents as agents
import manager_backend as backend


class ReviewedActions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cycode-manager-data-')
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name)
        self.home, self.project, self.user = (base / name for name in ('Core', 'project', 'user'))
        for path in (self.home, self.project, self.user): path.mkdir()
        skill = self.home / 'app/skills/cycode-core/SKILL.md'
        skill.parent.mkdir(parents=True)
        skill.write_text('---\nname: cycode-core\ndescription: Instructions\n---\n')
        (self.home / 'core-distribution.json').write_text(json.dumps({'format': 'cycode-core-distribution/1',
            'product': 'CyCodeCore', 'version': '0.1.4', 'state': 'complete', 'sourceRevision': 'a' * 40}))
        verifier = patch.object(agents, 'verify_payload', return_value=({}, 'hash'))
        verifier.start(); self.addCleanup(verifier.stop)
        self.manager = agents.Manager(self.home, state=base / 'state', user_home=self.user, appdata=self.user / 'AppData')
        self.backend = backend.Backend(self.manager, cache=base / 'cache')
        self.request = {'action': 'agents.prepare', 'clients': ['codex'], 'project': str(self.project), 'skill': False}

    def test_apply_requires_an_issued_unchanged_review_and_cannot_replay(self):
        review = self.backend.call(self.request)
        config = self.project / '.codex/config.toml'
        self.assertFalse(config.exists())
        self.backend.call({'action': 'agents.apply', 'plan': review['plan']})
        self.assertIn('CYCODE_MCP_PROFILE', config.read_text())
        with self.assertRaises((ValueError, OSError)):
            self.backend.call({'action': 'agents.apply', 'plan': review['plan']})

    def test_changed_configuration_requires_a_new_review(self):
        review = self.backend.call(self.request)
        config = self.project / '.codex/config.toml'
        config.parent.mkdir(); config.write_text('# changed after preview\n')
        with self.assertRaisesRegex(ValueError, 'changed after preview'):
            self.backend.call({'action': 'agents.apply', 'plan': review['plan']})
        self.assertEqual(config.read_text(), '# changed after preview\n')

    def test_expired_and_forged_tokens_are_refused(self):
        review = self.backend.call(self.request)
        file = self.manager.state / 'plans' / (review['plan'] + '.json')
        value = json.loads(file.read_text()); value['created'] = 0; file.write_text(json.dumps(value))
        for token in (review['plan'], '../escape'):
            with self.assertRaises(ValueError): self.backend.call({'action': 'agents.apply', 'plan': token})

    def test_preferences_preserve_unrelated_fields_and_validate_values(self):
        self.manager.state.mkdir()
        file = self.manager.state / 'preferences.json'
        file.write_text('{"retained":7}')
        self.backend.call({'action': 'preferences.save', 'preferences': {'language': 'fr', 'theme': 'violet', 'compact': True}})
        self.assertEqual(self.backend.preferences()['retained'], 7)
        with self.assertRaises(ValueError):
            self.backend.call({'action': 'preferences.save', 'preferences': {'language': 'invalid', 'theme': 'navy', 'compact': False}})

    def test_private_clangd_install_requires_explicit_confirmation(self):
        with self.assertRaisesRegex(ValueError, 'Review the source'):
            self.backend.call({'action': 'dependencies.install'})

    def test_update_rechecks_the_offer_and_keeps_the_selected_cache(self):
        with patch.object(backend.updates, 'check', return_value={'release': {'version': '0.1.5'}}) as check, \
             patch.object(backend.updates, 'install_update', return_value={'status': 'installed'}) as install:
            with self.assertRaises(ValueError):
                self.backend.call({'action': 'updates.install', 'version': '0.1.4', 'accepted': True})
            self.backend.call({'action': 'updates.install', 'version': '0.1.5', 'accepted': True})
            self.assertEqual(check.call_args.kwargs['cache'], self.backend.cache)
            self.assertEqual(install.call_args.kwargs['cache'], self.backend.cache)


if __name__ == '__main__': unittest.main()

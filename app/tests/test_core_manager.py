"""Manager file/installer contracts. No Core, native fixture or client is launched."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import tomllib
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from agent_config import Document
import core_agents as agents
import core_installation as installations


class JsonConfiguration(unittest.TestCase):
    def test_comments_strings_trailing_commas_and_other_servers_survive(self):
        original = '// keep\n{"servers": {"other": {"args": ["} // quoted",],},}, "inputs": []}\n'
        changed = Document(original, True).set(('servers', 'cycode_core'), {'command': 'python'})
        self.assertIn('"other": {"args": ["} // quoted",],}', changed)
        self.assertTrue(changed.startswith('// keep\n'))
        removed = Document(changed, True).set(('servers', 'cycode_core'), None, remove=True)
        self.assertEqual(Document(removed, True).root.value, Document(original, True).root.value)

    def test_remove_first_middle_last_and_only_member(self):
        for key in ('a', 'b', 'c'):
            text = '{"a": 1, /* comment */ "b": 2, "c": 3}'
            result = Document(text, True).set((key,), None, remove=True)
            self.assertEqual(Document(result, True).root.value, {k: v for k, v in {'a': 1, 'b': 2, 'c': 3}.items() if k != key})
        self.assertEqual(Document(Document('{"a":1}').set(('a',), None, remove=True)).root.value, {})

    def test_nested_insert_does_not_replace_other_members(self):
        text = '{"servers":{"keep":7}}'
        result = Document(text).set(('servers', 'nested', 'new'), 2)
        self.assertEqual(Document(result).root.value, {'servers': {'keep': 7, 'nested': {'new': 2}}})

    def test_duplicate_malformed_and_excessive_inputs_refused(self):
        for text in ('{"a":1,"a":2}', '{"a": NaN}', '{"a":1,}', '{/*comment*/}', '[]'):
            with self.assertRaises(ValueError):
                Document(text)
        with self.assertRaises(ValueError):
            Document('{"a":' * 40 + '{}' + '}' * 40)


class ManagerFiles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'Core'
        self.home = self.payload('0.1.4', 'a')
        self.project = self.base / 'project'
        self.project.mkdir()
        self.user = self.base / 'user'
        self.user.mkdir()
        self.state = self.base / 'manager-state'
        self.verifier = patch.object(agents, 'verify_payload', return_value=({}, 'hash')).start()
        self.addCleanup(patch.stopall)
        self.manager = self.context(self.home)

    def payload(self, version, token):
        home = self.root / 'versions' / (version + '-' + token * 32)
        source = home / 'app/skills/cycode-core/SKILL.md'
        source.parent.mkdir(parents=True)
        source.write_text('---\nname: cycode-core\ndescription: Core instructions\n---\n', encoding='utf-8')
        (home / 'core-distribution.json').write_text(json.dumps({'format': 'cycode-core-distribution/1',
            'product': 'CyCodeCore', 'version': version, 'state': 'complete', 'sourceRevision': token * 40}), encoding='utf-8')
        return home

    def context(self, home):
        return agents.Manager(home, state=self.state, user_home=self.user, appdata=self.user / 'AppData/Roaming')

    def test_all_client_profiles_and_skill_only_without_project(self):
        plan = self.manager.prepare(list(agents.CLIENTS), None, scope='user', skill=True, mcp=False)
        self.assertFalse(self.state.exists())
        self.manager.apply(plan)
        self.assertTrue((self.user / '.agents/skills/cycode-core/SKILL.md').is_file())
        self.assertTrue((self.user / '.claude/skills/cycode-core/SKILL.md').is_file())
        self.assertTrue((self.user / '.copilot/skills/cycode-core/SKILL.md').is_file())
        self.assertTrue(plan.skipped)
        self.assertEqual(self.manager.receipts(), [])

    def test_all_project_clients_preserve_settings_and_share_claude_copilot_entry(self):
        clients = [key for key, client in agents.CLIENTS.items() if client.project_config]
        path = self.project / '.vscode/mcp.json'
        path.parent.mkdir()
        path.write_text('// retained\n{"servers":{"other":{"command":"keep"}},"inputs":[],}', encoding='utf-8')
        plan = self.manager.prepare(clients, self.project)
        self.assertFalse((self.project / '.codex').exists())
        self.manager.apply(plan)
        config = tomllib.loads((self.project / '.codex/config.toml').read_text('utf-8'))
        server = config['mcp_servers']['cycode_core']
        self.assertEqual(server['env']['CYCODE_PERMISSIONS'], '')
        self.assertIn('agent_host.py', ' '.join(server['args']))
        self.assertEqual(len(json.loads((self.project / '.mcp.json').read_text())['mcpServers']), 1)
        text = path.read_text()
        self.assertIn('// retained', text)
        self.assertIn('"other":{"command":"keep"}', text)
        self.assertEqual(len(self.manager.receipts()), len(clients) - 1)

    def test_refresh_changes_generation_preserves_permissions_and_unrelated_settings(self):
        self.manager.apply(self.manager.prepare(['codex', 'cursor'], self.project, access='edit', mcp_profile='full'))
        codex = self.project / '.codex/config.toml'
        codex.write_text('model="keep"\n' + codex.read_text(), encoding='utf-8')
        second = self.context(self.payload('0.1.5', 'b'))
        second.apply(second.prepare_refresh())
        server = tomllib.loads(codex.read_text())['mcp_servers']['cycode_core']
        self.assertIn(str(second.home), server['command'])
        self.assertEqual(server['env']['CYCODE_PERMISSIONS'], 'workspace.write,process.execute')
        self.assertEqual(server['env']['CYCODE_MCP_PROFILE'], 'full')
        self.assertEqual(tomllib.loads(codex.read_text())['model'], 'keep')
        self.assertTrue(self.home.exists())
        self.assertTrue(list(self.state.rglob('*.before')))

    def test_old_portable_launcher_resolves_the_new_selected_generation(self):
        selected = self.payload('0.1.5', 'b')
        (self.root / 'current.json').write_text(json.dumps({
            'format': 'cycode-core-installation/1', 'product': 'CyCodeCore', 'version': '0.1.5',
            'payload': 'versions/' + selected.name,
            'manifestSha256': hashlib.sha256((selected / 'core-distribution.json').read_bytes()).hexdigest()}))
        reopened = self.context(self.home)
        self.assertEqual(reopened.home, selected.resolve())

    def test_unowned_or_edited_servers_and_skill_are_preserved(self):
        path = self.project / '.cursor/mcp.json'
        path.parent.mkdir()
        original = '{"mcpServers":{"cycode_core":{"command":"personal"}}}'
        path.write_text(original)
        with self.assertRaises(ValueError):
            self.manager.prepare(['cursor'], self.project)
        self.assertEqual(path.read_text(), original)
        self.assertFalse((self.project / '.cursor/skills').exists())
        self.manager.apply(self.manager.prepare(['codex'], self.project))
        codex = self.project / '.codex/config.toml'
        codex.write_text(codex.read_text().replace('enabled = true', 'enabled = false'))
        plan = self.context(self.payload('0.1.5', 'b')).prepare_refresh()
        self.assertTrue(plan.skipped)
        self.assertEqual(plan.writes, {})

    def test_review_guard_prevents_any_write_after_external_change(self):
        plan = self.manager.prepare(['codex'], self.project)
        config = self.project / '.codex/config.toml'
        config.parent.mkdir()
        config.write_text('model="external"')
        with self.assertRaises(ValueError):
            self.manager.apply(plan)
        self.assertFalse((self.project / '.agents/skills').exists())
        self.assertEqual(config.read_text(), 'model="external"')

    def test_legacy_registration_can_be_refreshed_then_removed_without_rediscovery(self):
        config = self.project / '.codex/config.toml'
        config.parent.mkdir()
        block = agents.shared.block_for(self.home, self.project, Path(sys.executable), self.state / 'old-jobs', 'read', False)
        config.write_text(block)
        old = {'format': 'cycode-agent-connection/1', 'home': str(self.home), 'workspace': str(self.project),
               'config': str(config), 'managedBlockSha256': agents.fingerprint(block), 'access': 'read', 'enabled': False}
        receipt = self.home / 'agent-state/old/connection.json'
        receipt.parent.mkdir(parents=True)
        receipt.write_text(json.dumps(old))
        second = self.context(self.payload('0.1.5', 'b'))
        second.apply(second.prepare_refresh())
        self.assertFalse(tomllib.loads(config.read_text())['mcp_servers']['cycode_core']['enabled'])
        row = second.receipts()[0]
        second.apply(second.prepare_remove(row['id']))
        self.assertEqual(second.receipts(), [])
        self.assertNotIn('cycode_core', tomllib.loads(config.read_text()).get('mcp_servers', {}))

    def test_failure_rolls_back_only_the_exact_manager_writes(self):
        plan = agents.Plan()
        a, b = self.base / 'a.txt', self.base / 'b.txt'
        a.write_bytes(b'old a')
        b.write_bytes(b'old b')
        plan.put(a, b'new a', b'old a')
        plan.put(b, b'new b', b'old b')
        atomic = agents.shared.atomic
        def fail(path, data, expected):
            if path == b and data == b'new b':
                raise OSError('simulated write error')
            return atomic(path, data, expected)
        with patch.object(agents.shared, 'atomic', side_effect=fail):
            with self.assertRaises(OSError):
                self.manager.apply(plan)
        self.assertEqual(a.read_bytes(), b'old a')
        self.assertEqual(b.read_bytes(), b'old b')


class ActivationContracts(unittest.TestCase):
    def test_switch_never_changes_an_older_open_payload_and_rejects_downgrade(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def create(number, token):
                path = root / 'versions' / (number + '-' + token * 32)
                path.mkdir(parents=True)
                record = {'format': 'cycode-core-distribution/1', 'product': 'CyCodeCore',
                          'state': 'complete', 'version': number, 'sourceRevision': token * 40}
                payload = json.dumps(record).encode()
                (path / 'core-distribution.json').write_bytes(payload)
                return path, record, hashlib.sha256(payload).hexdigest()
            old, record, digest = create('0.1.4', 'a')
            with patch.object(installations, 'verify_payload', return_value=(record, digest)):
                installations.activate(root, old, '0.1.4')
            sentinel = old / 'held.txt'
            sentinel.write_bytes(b'old running payload remains intact')
            new, record, digest = create('0.1.5', 'b')
            with sentinel.open('rb') as held, patch.object(installations, 'verify_payload', return_value=(record, digest)):
                installations.activate(root, new, '0.1.5')
                self.assertEqual(held.read(), b'old running payload remains intact')
            self.assertEqual(installations.current_payload(root), new.resolve())
            before = (root / 'current.json').read_bytes()
            old_record, old_digest = installations.manifest(old)
            with patch.object(installations, 'verify_payload', return_value=(old_record, old_digest)):
                with self.assertRaises(ValueError):
                    installations.activate(root, old, '0.1.4')
            self.assertEqual((root / 'current.json').read_bytes(), before)

    def test_corrupt_pointer_or_escaped_payload_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'current.json').write_text('{"format":"cycode-core-installation/1","product":"CyCodeCore","payload":"../outside"}')
            with self.assertRaises(ValueError):
                installations.current_payload(root)

    def test_unrecognised_pointer_is_preserved_even_when_empty(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            home = root / 'versions' / ('0.1.4-' + 'a' * 32)
            home.mkdir(parents=True)
            for data in (b'{}', b'[]', b'null', b''):
                (root / 'current.json').write_bytes(data)
                with patch.object(installations, 'verify_payload', return_value=({'version': '0.1.4'}, 'a' * 64)):
                    with self.assertRaises(ValueError):
                        installations.activate(root, home, '0.1.4')
                self.assertEqual((root / 'current.json').read_bytes(), data)

    def test_invalid_fingerprint_is_a_reportable_validation_error(self):
        for fingerprint in (None, 4, [], 'not-a-hash'):
            with self.assertRaises(ValueError):
                installations.pointer_parts({'format': 'cycode-core-installation/1', 'product': 'CyCodeCore',
                    'payload': 'versions/0.1.4-' + 'a' * 32, 'version': '0.1.4', 'manifestSha256': fingerprint})


if __name__ == '__main__':
    unittest.main()

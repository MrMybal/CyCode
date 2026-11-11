"""Agent installation/data regressions. Never generate or execute a test binary."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import tomllib
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
import install_core_agents as install
import package_core as package


class AgentInstallation(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.home = self.root / 'distribution'
        self.workspace = self.root / 'project'
        self.workspace.mkdir()
        self.skills = self.root / 'user-skills'
        for name in ('app/tool.py', 'app/core_host.py', 'app/contracts.py', 'app/CyTool.json',
                     'app/MCPMethods.json', 'app/skills/cycode-core/SKILL.md'):
            target = self.home / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('{}' if name.endswith('.json') else 'source text\n', encoding='utf-8')

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, **kwargs):
        return install.configure(self.home, self.workspace, Path(sys.executable), self.skills, **kwargs)

    def test_register_update_preserves_unrelated_settings_and_limits_permissions(self):
        path = self.workspace / '.codex/config.toml'
        path.parent.mkdir()
        original = 'model = "existing-model"\n[mcp_servers.other]\ncommand = "other"\n'
        path.write_text(original, encoding='utf-8')
        first = self.configure()
        self.assertFalse(first['enabled'])
        self.assertFalse(first['nativePresent'])
        self.assertFalse(first['executed'])
        before = path.read_bytes()
        self.assertEqual(self.configure(), first)
        self.assertEqual(before, path.read_bytes())
        # Changing an unrelated setting is permitted; do not erase it on update.
        path.write_text(path.read_text('utf-8').replace('existing-model', 'changed-model'), encoding='utf-8')
        self.configure(access='edit')
        result = tomllib.loads(path.read_text('utf-8'))
        self.assertEqual(result['model'], 'changed-model')
        self.assertEqual(result['mcp_servers']['other']['command'], 'other')
        server = result['mcp_servers']['cycode_core']
        self.assertEqual(server['env']['CYCODE_PERMISSIONS'], 'workspace.write,process.execute')
        self.assertIn('--disable-native-plugins', server['args'])
        self.assertFalse(server['enabled'])
        self.assertTrue(list((self.home / 'agent-state').rglob('config-before-*.toml')))

    def test_existing_unowned_server_is_preserved_before_skill_install(self):
        path = self.workspace / '.codex/config.toml'
        path.parent.mkdir()
        before = b'[mcp_servers.cycode_core]\ncommand = "owned by someone else"\n'
        path.write_bytes(before)
        with self.assertRaisesRegex(ValueError, 'Existing cycode_core'):
            self.configure()
        self.assertEqual(path.read_bytes(), before)
        self.assertFalse(self.skills.exists())

    def test_explicit_client_config_has_separate_receipt_and_preserves_other_servers(self):
        global_config = self.root / 'user/config.toml'
        global_config.parent.mkdir()
        global_config.write_text('model = "keep"\n[mcp_servers.other]\nurl = "https://example.invalid/mcp"\n', encoding='utf-8')
        self.configure()
        local_before = (self.workspace / '.codex/config.toml').read_bytes()
        self.configure(config_path=global_config, access='edit')
        self.assertEqual((self.workspace / '.codex/config.toml').read_bytes(), local_before)
        parsed = tomllib.loads(global_config.read_text('utf-8'))
        self.assertEqual(parsed['model'], 'keep')
        self.assertEqual(parsed['mcp_servers']['other']['url'], 'https://example.invalid/mcp')
        self.assertEqual(len(list((self.home / 'agent-state').rglob('connection.json'))), 2)

    def test_versioned_install_preserves_receipt_ownership_and_unrelated_settings(self):
        self.configure()
        next_home=self.root/'next-version'
        import shutil
        shutil.copytree(self.home,next_home)
        result=install.configure(next_home,self.workspace,Path(sys.executable),self.skills,
                                 state_dir=self.home/'agent-state')
        parsed=tomllib.loads((self.workspace/'.codex/config.toml').read_text('utf-8'))
        self.assertEqual(parsed['mcp_servers']['cycode_core']['env']['CYCODE_CORE_HOME'],str(next_home.resolve()))
        self.assertEqual(result['home'],str(next_home.resolve()))
        self.assertFalse(result['executed'])
        next_config=parsed['mcp_servers']['cycode_core']
        data_path=next_config['args'][next_config['args'].index('--data-dir')+1]
        self.assertTrue(Path(data_path).resolve().is_relative_to(next_home.resolve()))
        self.assertEqual(install.configure(next_home,self.workspace,Path(sys.executable),self.skills),result)
    def test_locally_edited_managed_block_is_not_overwritten(self):
        self.configure()
        path = self.workspace / '.codex/config.toml'
        path.write_text(path.read_text('utf-8').replace('enabled = false', 'enabled = true'), encoding='utf-8')
        before = path.read_bytes()
        with self.assertRaisesRegex(ValueError, 'edited managed MCP'):
            self.configure()
        self.assertEqual(path.read_bytes(), before)

    def test_modified_skill_is_preserved(self):
        self.configure()
        target = self.skills / 'cycode-core/SKILL.md'
        target.write_text('personal changes\n', encoding='utf-8')
        source = self.home / 'app/skills/cycode-core/SKILL.md'
        source.write_text('new upstream\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'locally modified skill'):
            self.configure(access='edit')
        self.assertEqual(target.read_text('utf-8'), 'personal changes\n')

    def test_enable_refuses_missing_native_before_any_installation_write(self):
        with self.assertRaisesRegex(ValueError, 'Native Core is absent'):
            self.configure(enabled=True)
        self.assertFalse((self.workspace / '.codex').exists())
        self.assertFalse(self.skills.exists())

    def test_corrupt_toml_and_redirected_path_fail_without_overwriting(self):
        path = self.workspace / '.codex/config.toml'
        path.parent.mkdir()
        path.write_bytes(b'not = [valid')
        with self.assertRaises(ValueError):
            self.configure()
        self.assertEqual(path.read_bytes(), b'not = [valid')


class CorePackage(unittest.TestCase):
    def test_agents_archive_cannot_be_installer(self):
        with patch.object(package.subprocess, 'run', side_effect=AssertionError('No compiler allowed')):
            with self.assertRaisesRegex(ValueError, 'agents-only'):
                package.installer({'core-distribution.json': b'{"state":"agents-only"}'}, '0.1.0', Path('.'))

    def test_archive_rejects_traversal_before_writing(self):
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder) / 'archive.zip'
            with self.assertRaises(ValueError):
                package.write_archive(target, {'../secret': b'private'}, '0.1.0')
            self.assertFalse(target.exists())

    def test_manifest_records_explicit_incomplete_state_and_actual_hashes(self):
        entries = {'app/CyTool.json': b'{"version":"0.2.3"}', 'app/tool.py': b'source'}
        record = package.manifest(entries, '0.1.0', 'a' * 40, agents_only=True)
        self.assertEqual(record['state'], 'agents-only')
        self.assertEqual(record['version'], '0.1.0')
        self.assertEqual(record['engineVersion'], '0.2.3')
        self.assertFalse(record['nativeExecutedByPackager'])
        self.assertEqual(next(r for r in record['files'] if r['path'] == 'app/tool.py')['sha256'], package.sha(b'source'))


if __name__ == '__main__':
    unittest.main()

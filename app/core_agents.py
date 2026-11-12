"""Client-specific agent configuration, owned skills and reviewed file changes.

This configures clients only. CyToolsCore remains the sole SDK/MCP runtime.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import json
import os
from pathlib import Path
import re
import tomllib
import uuid

from agent_config import Document
from core_installation import current_payload, installation_root, verify_payload, management_lock
from scripts import install_core_agents as shared


@dataclass(frozen=True)
class Client:
    name: str
    project_config: str | None
    user_config: str
    skill_directory: str | None
    servers_key: str = 'mcpServers'
    comments: bool = False


CLIENTS = {
    'codex': Client('Codex', '.codex/config.toml', '.codex/config.toml', '.agents/skills', 'mcp_servers'),
    'claude-code': Client('Claude Code', '.mcp.json', '.claude.json', '.claude/skills'),
    'cursor': Client('Cursor', '.cursor/mcp.json', '.cursor/mcp.json', '.cursor/skills'),
    'copilot': Client('VS Code / Copilot', '.mcp.json', '.copilot/mcp-config.json', '.github/skills'),
    'vscode-local': Client('VS Code — Local (legacy)', '.vscode/mcp.json', '@appdata/Code/User/mcp.json', '.github/skills', 'servers', True),
    'gemini': Client('Gemini CLI', '.gemini/settings.json', '.gemini/settings.json', '.gemini/skills'),
    'claude-desktop': Client('Claude Desktop (MCP)', None, '@appdata/Claude/claude_desktop_config.json', None),
}


def manager_state(root):
    base = Path(os.environ.get('LOCALAPPDATA', Path.home() / '.local/share'))
    key = shared.digest(os.path.normcase(str(root)).encode('utf-8'))[:20]
    return shared.ordinary(base / 'CyCodeCore/manager' / key)


def fingerprint(text):
    return shared.digest(text.replace('\r\n', '\n').encode('utf-8'))


@dataclass
class Plan:
    writes: dict = field(default_factory=dict)
    summary: list = field(default_factory=list)
    skipped: list = field(default_factory=list)

    def put(self, path, after, before):
        path = shared.ordinary(path)
        if path in self.writes and self.writes[path] != (after, before):
            raise ValueError('Two selected clients require different changes to the same file: ' + str(path))
        self.writes[path] = (after, before)

    def preview(self):
        return {'changes': self.summary, 'skipped': self.skipped,
                'files': [str(p) for p, (a, b) in self.writes.items() if a != b], 'executed': False}


class Manager:
    def __init__(self, home, *, root=None, state=None, user_home=None, appdata=None):
        self.root = shared.ordinary(root or installation_root(home)).resolve(strict=True)
        self.home = (current_payload(self.root) if root or (self.root / 'current.json').exists()
                     else shared.ordinary(home).resolve(strict=True))
        self.state = shared.ordinary(state or manager_state(self.root))
        self.user = shared.ordinary(user_home or Path.home()).resolve()
        self.appdata = shared.ordinary(appdata or Path(os.environ.get('APPDATA', self.user / 'AppData/Roaming'))).resolve()

    def locations(self, client, workspace, scope):
        profile = CLIENTS[client]
        if scope not in ('user', 'project'):
            raise ValueError('Choose user or project scope')
        if scope == 'project' and workspace is None:
            raise ValueError('An existing project is required for project scope')
        if scope == 'project' and profile.project_config is None:
            raise ValueError(profile.name + ' only supports user-level MCP configuration')
        location = profile.project_config if scope == 'project' else profile.user_config
        base = workspace if scope == 'project' else self.user
        if location.startswith('@appdata/'):
            config = self.appdata / location[len('@appdata/'):]
        else:
            config = base / location
        skills = None
        if profile.skill_directory:
            directory = profile.skill_directory
            if scope == 'user' and client in ('copilot', 'vscode-local'):
                directory = '.copilot/skills'
            if scope == 'user' and client == 'codex':
                legacy = self.user / '.codex/skills/cycode-core/.cycode-skill.json'
                if legacy.is_file():
                    directory = '.codex/skills'  # Keep an already managed legacy skill in place.
            skills = shared.ordinary(base / directory)
        return shared.ordinary(config), skills

    def receipts(self):
        result = {}
        folder = self.state / 'connections'
        if folder.exists():
            paths = sorted(folder.glob('*.json'))
            if len(paths) > 512:
                raise ValueError('More than 512 managed connections; choose a narrower installation')
            for path in paths:
                record = json.loads(shared.bounded(path))
                if (not isinstance(record, dict) or record.get('format') != 'cycode-manager-connection/1'
                        or record.get('root') != str(self.root)
                        or not isinstance(record.get('id'), str) or not re.fullmatch('[a-f0-9]{24}', record['id'])
                        or record['id'] != path.stem or record.get('client') not in CLIENTS
                        or record.get('access') not in ('read', 'edit') or record.get('scope') not in ('user', 'project')
                        or type(record.get('enabled')) is not bool
                        or any(not isinstance(record.get(key), str) or not record[key] for key in ('home', 'workspace', 'config', 'server'))):
                    raise ValueError('Invalid manager receipt: ' + str(path))
                record['_receipt'] = str(path)
                result[record['id']] = record
        # Discover old issued registrations without writing configurations or receipts.
        homes = [self.root]
        versions = self.root / 'versions'
        if versions.is_dir():
            homes += [shared.ordinary(p) for p in sorted(versions.iterdir()) if p.is_dir() and not p.is_symlink()][:128]
        for home in homes:
            folder = shared.ordinary(home / 'agent-state')
            if not folder.is_dir():
                continue
            paths = sorted(folder.glob('*/connection.json'))
            if len(paths) > 512:
                raise ValueError('Too many legacy registrations')
            for path in paths:
                old = json.loads(shared.bounded(path))
                if (not isinstance(old, dict) or old.get('format') != 'cycode-agent-connection/1'
                        or any(not isinstance(old.get(key), str) for key in ('home', 'config', 'workspace', 'managedBlockSha256'))
                        or old.get('access') not in ('read', 'edit') or type(old.get('enabled')) is not bool
                        or shared.ordinary(old['home']).resolve() != home.resolve()):
                    continue
                config = shared.ordinary(old['config']).resolve()
                workspace = shared.ordinary(old['workspace']).resolve()
                identity = shared.digest((os.path.normcase(str(config)) + '\ncycode_core').encode())[:24]
                if identity in result:
                    continue
                result[identity] = {'format': 'cycode-manager-connection/1', 'id': identity, 'client': 'codex',
                    'root': str(self.root), 'home': str(home), 'workspace': str(workspace), 'config': str(config),
                    'server': 'cycode_core', 'scope': 'project' if config == workspace / '.codex/config.toml' else 'user',
                    'access': old['access'], 'enabled': old['enabled'], 'managedHash': old['managedBlockSha256'],
                    'begin': shared.BEGIN, 'end': shared.END, 'legacy': True}
        return [r for r in result.values() if not r.get('removed')]

    def _block(self, record, command, arguments, environment):
        quote = lambda value: json.dumps(value, ensure_ascii=False)
        return '\n'.join([record['begin'], '[mcp_servers.' + record['server'] + ']',
            'command = ' + quote(command), 'args = ' + quote(arguments),
            'enabled = ' + str(record.get('enabled', True)).lower(), 'startup_timeout_sec = 60', 'tool_timeout_sec = 120',
            '[mcp_servers.' + record['server'] + '.env]',
            *[key + ' = ' + quote(value) for key, value in environment.items()], record['end']])

    def _config(self, record, *, remove=False):
        if not remove and not shared.ordinary(record['workspace']).is_dir():
            raise ValueError('Project no longer exists: ' + record['workspace'])
        path = shared.ordinary(record['config'])
        before = shared.bounded(path) if path.exists() else None
        text = before.decode('utf-8-sig') if before is not None else ''
        bom = b'\xef\xbb\xbf' if before and before.startswith(b'\xef\xbb\xbf') else b''
        profile = CLIENTS[record['client']]
        generation = shared.digest(str(self.home).encode())[:16]
        jobs = self.state / 'jobs' / record['id'] / generation
        command = str(self.home / 'tools/python/python.exe')
        arguments = ['-B', '-X', 'utf8', str(self.home / 'app/agent_host.py'),
                     '--workspace', record['workspace'], '--state', str(jobs)]
        environment = {'CYCODE_CORE_HOME': str(self.home),
                       'CYCODE_PERMISSIONS': 'workspace.write,process.execute' if record['access'] == 'edit' else ''}
        expected = record.get('managedHash')
        if record['client'] == 'codex':
            parsed = tomllib.loads(text)
            begin, end = record['begin'], record['end']
            if text.count(begin) != text.count(end) or text.count(begin) > 1:
                raise ValueError('Malformed managed Codex markers; settings preserved')
            block = self._block(record, command, arguments, environment)
            if begin in text:
                start = text.index(begin)
                finish = text.index(end, start) + len(end)
                if not expected or fingerprint(text[start:finish]) != expected:
                    raise ValueError('Locally modified Codex connection preserved: ' + str(path))
                result = text[:start] + ('' if remove else block) + text[finish:]
            else:
                if expected or record['server'] in parsed.get('mcp_servers', {}):
                    raise ValueError('Missing or unowned Codex connection preserved: ' + str(path))
                result = text.rstrip() + ('\n\n' if text.strip() else '') + block + '\n'
            tomllib.loads(result)
            owned_hash = fingerprint(block)
        else:
            document = Document(text or '{}', profile.comments)
            keys = (profile.servers_key, record['server'])
            old = document.raw(keys)
            if (old is not None and (not expected or fingerprint(old) != expected)) or (old is None and expected):
                raise ValueError('Unowned or locally modified connection preserved: ' + str(path))
            entry = {'type': 'stdio', 'command': command, 'args': arguments, 'env': environment}
            result = document.set(keys, entry, remove=remove)
            owned_hash = fingerprint(Document(result, profile.comments).raw(keys)) if not remove else None
        return path, bom + result.encode('utf-8'), before, owned_hash

    def _connection(self, plan, record, *, remove=False):
        path, after, before, owned_hash = self._config(record, remove=remove)
        plan.put(path, after, before)
        receipt = self.state / 'connections' / (record['id'] + '.json')
        previous = shared.bounded(receipt) if receipt.exists() else None
        if remove:
            tombstone = {k: v for k, v in record.items() if not k.startswith('_')}
            tombstone['removed'] = True
            plan.put(receipt, (json.dumps(tombstone, indent=2) + '\n').encode(), previous)
        else:
            record = {k: v for k, v in record.items() if not k.startswith('_')}
            record.update(home=str(self.home), managedHash=owned_hash, legacy=False)
            plan.put(receipt, (json.dumps(record, indent=2) + '\n').encode(), previous)
        plan.summary.append({'client': CLIENTS[record['client']].name, 'config': str(path),
                             'project': record['workspace'], 'action': 'remove' if remove else 'connect',
                             'access': record['access'], 'reloadRequired': True})

    def prepare(self, clients, workspace=None, *, scope='project', skill=True, mcp=True, access='read'):
        if not clients or len(set(clients)) != len(clients) or any(c not in CLIENTS for c in clients):
            raise ValueError('Choose supported agent applications')
        if access not in ('read', 'edit') or not (skill or mcp):
            raise ValueError('Choose skill and/or MCP with explicit access')
        workspace = shared.ordinary(workspace).resolve(strict=True) if workspace else None
        if (mcp or scope == 'project') and (workspace is None or not workspace.is_dir()):
            raise ValueError('Choose an existing project for the MCP connection')
        verify_payload(self.home)
        plan = Plan()
        owned = {row['id']: row for row in self.receipts()}
        for client in clients:
            if scope == 'project' and CLIENTS[client].project_config is None:
                plan.skipped.append(CLIENTS[client].name + ': select user scope for this application')
                continue
            config, skills = self.locations(client, workspace, scope)
            if skill and skills is not None:
                target, writes = shared.prepare_skill(self.home, skills)
                for path, after, before in writes:
                    plan.put(path, after, before)
                plan.summary.append({'client': CLIENTS[client].name, 'skill': str(target), 'action': 'skill'})
            elif skill:
                plan.skipped.append(CLIENTS[client].name + ': no local SKILL.md directory; MCP only')
            if not mcp:
                continue
            server = 'cycode_core' if scope == 'project' else 'cycode_core_' + shared.digest(str(workspace).encode())[:8]
            existing = next((r for r in owned.values() if r['config'] == str(config)
                             and r['workspace'] == str(workspace) and r['client'] == client), None)
            if existing:
                server = existing['server']
            identity = shared.digest((os.path.normcase(str(config)) + '\n' + server).encode())[:24]
            record = owned.get(identity) or {'format': 'cycode-manager-connection/1', 'id': identity,
                'client': client, 'root': str(self.root), 'config': str(config), 'server': server,
                'begin': '# BEGIN CyCodeCore managed MCP: ' + identity,
                'end': '# END CyCodeCore managed MCP: ' + identity}
            record = {**record, 'workspace': str(workspace), 'scope': scope, 'access': access, 'enabled': True}
            # Claude Code and Copilot share .mcp.json; one identical managed server suffices.
            if identity in {p.stem for p in plan.writes if p.parent == self.state / 'connections'}:
                continue
            self._connection(plan, record)
        return plan

    def prepare_refresh(self):
        verify_payload(self.home)
        plan = Plan()
        for record in self.receipts():
            try:
                # Do not escalate permissions, enable a disabled connection or rewrite a personal skill.
                one = Plan()
                self._connection(one, record)
                for path, (after, before) in one.writes.items():
                    plan.put(path, after, before)
                plan.summary.extend(one.summary)
            except (ValueError, OSError) as error:
                plan.skipped.append(str(error))
        return plan

    def prepare_remove(self, identity):
        record = next((r for r in self.receipts() if r['id'] == identity), None)
        if record is None:
            raise ValueError('Unknown managed connection')
        plan = Plan()
        self._connection(plan, record, remove=True)
        return plan

    def apply(self, plan):
        with management_lock(self.state):
            return self._apply(plan)

    def _apply(self, plan):
        writes = [(p, a, b) for p, (a, b) in plan.writes.items() if a != b]
        for path, _, before in writes:
            current = shared.bounded(path, 4 * shared.MAX_CONFIG) if path.exists() else None
            if current != before:
                raise ValueError('Settings changed after review; prepare again: ' + str(path))
        if not writes:
            return plan.preview()
        backup = self.state / 'backups' / uuid.uuid4().hex
        backup.mkdir(parents=True, mode=0o700, exist_ok=False)
        journal = []
        for index, (path, after, before) in enumerate(writes):
            if before is not None:
                (backup / (str(index) + '.before')).write_bytes(before)
            journal.append({'path': str(path), 'before': shared.digest(before) if before is not None else None,
                            'after': shared.digest(after) if after is not None else None})
        (backup / 'changes.json').write_text(json.dumps(journal, indent=2), encoding='utf-8')
        committed = []
        try:
            for path, after, before in writes:
                if after is None:
                    if before is not None:
                        if shared.bounded(path) != before:
                            raise ValueError('Connection receipt changed')
                        path.unlink()
                else:
                    shared.atomic(path, after, before)
                committed.append((path, after, before))
        except Exception:
            for path, after, before in reversed(committed):
                current = shared.bounded(path, 4 * shared.MAX_CONFIG) if path.exists() else None
                if current == after:
                    if before is None:
                        path.unlink(missing_ok=True)
                    else:
                        shared.atomic(path, before, after)
            raise
        return {**plan.preview(), 'backup': str(backup), 'applied': True}

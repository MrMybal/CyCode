"""Install Core's skill and project-scoped Codex connection; never start a payload."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import tomllib

ROOT = Path(__file__).resolve().parents[2]
BEGIN = '# BEGIN CyCode Core managed MCP'
END = '# END CyCode Core managed MCP'
SERVER = 'cycode_core'
MAX_CONFIG = 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def ordinary(path):
    """Refuse symlinks and Windows junctions in installation write paths."""
    path = Path(path).absolute()
    for item in [path, *path.parents]:
        if item.exists() or item.is_symlink():
            st = item.lstat()
            if item.is_symlink() or getattr(st, 'st_file_attributes', 0) & 1024:
                raise ValueError('Redirected path refused: ' + str(item))
    return path


def bounded(path, maximum=MAX_CONFIG):
    path = ordinary(path)
    if not path.is_file() or path.stat().st_size > maximum:
        raise ValueError('Missing or oversized input: ' + str(path))
    return path.read_bytes()


def atomic(path, data, expected):
    path = ordinary(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    current = bounded(path, 4 * MAX_CONFIG) if path.exists() else None
    if current != expected:
        raise ValueError('Changed destination preserved: ' + str(path))
    fd, temporary = tempfile.mkstemp(prefix='.cycode-agent-', dir=path.parent)
    temporary = Path(temporary)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        # Compare again immediately before replacing a configuration/skill file.
        current = bounded(path, 4 * MAX_CONFIG) if path.exists() else None
        if current != expected:
            raise ValueError('Concurrent change preserved: ' + str(path))
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def managed_config(before, block, previous_hash=None):
    text = before.decode('utf-8-sig') if before is not None else ''
    parsed = tomllib.loads(text)
    if text.count(BEGIN) != text.count(END) or text.count(BEGIN) > 1:
        raise ValueError('Malformed managed MCP markers; existing settings preserved')
    if BEGIN in text:
        start = text.index(BEGIN)
        finish = text.index(END, start) + len(END)
        old = text[start:finish]
        if previous_hash != digest(old.replace('\r\n', '\n').encode('utf-8')):
            raise ValueError('Unowned or edited managed MCP settings preserved')
        result = text[:start] + block + text[finish:]
    else:
        if SERVER in parsed.get('mcp_servers', {}):
            raise ValueError('Existing cycode_core server preserved; review it explicitly')
        result = text.rstrip() + ('\n\n' if text.strip() else '') + block + '\n'
    tomllib.loads(result)
    return result.encode('utf-8')


def block_for(home, workspace, interpreter, data, access, enabled):
    quote = lambda value: json.dumps(str(value), ensure_ascii=False)
    arguments = ['-B', '-X', 'utf8', str(home / 'app/tool.py'), '--core-workspace',
                 str(workspace), '--core-executable', str(home / 'cycode_cli.exe'),
                 '--disable-native-plugins', '--data-dir', str(data), 'mcp']
    return '\n'.join([BEGIN, '[mcp_servers.cycode_core]',
        'command = ' + quote(interpreter), 'args = ' + json.dumps(arguments, ensure_ascii=False),
        'enabled = ' + str(enabled).lower(), 'startup_timeout_sec = 60',
        'tool_timeout_sec = 120', '[mcp_servers.cycode_core.env]',
        'CYCODE_CORE_HOME = ' + quote(home),
        'CYCODE_PERMISSIONS = ' + quote('workspace.write,process.execute' if access == 'edit' else ''), END])


def validate_home(home, enabled):
    for name in ('app/tool.py', 'app/core_host.py', 'app/contracts.py', 'app/CyTool.json',
                 'app/MCPMethods.json', 'app/skills/cycode-core/SKILL.md'):
        bounded(home / name, 4 * MAX_CONFIG)
    if enabled:
        binary = ordinary(home / 'cycode_cli.exe')
        if not binary.is_file():
            raise ValueError('Native Core is absent: cannot enable an agents-only installation')
        # Check an issued package inventory, not merely an arbitrary file name.
        manifest = json.loads(bounded(home / 'core-distribution.json'))
        row = next((r for r in manifest.get('files', []) if r.get('path') == 'cycode_cli.exe'), None)
        if (manifest.get('state') != 'complete' or not row or binary.stat().st_size != row['size'] or
                digest(binary.read_bytes()) != row['sha256']):
            raise ValueError('Native Core does not match a complete installation manifest')


def prepare_skill(home, destination):
    """Return guarded writes for the same skill installer, without changing files."""
    source = home / 'app/skills/cycode-core'
    files = {p.relative_to(source).as_posix(): bounded(p) for p in sorted(source.rglob('*')) if p.is_file()}
    if 'SKILL.md' not in files:
        raise ValueError('Incomplete skill source')
    target = ordinary(destination / 'cycode-core')
    marker = target / '.cycode-skill.json'
    previous = json.loads(bounded(marker)) if marker.exists() else {'files': {}}
    if target.exists() and not marker.exists():
        raise ValueError('An existing unowned skill is preserved: ' + str(target))
    writes = []
    for name, payload in files.items():
        path = target / name
        old = bounded(path) if path.exists() else None
        if old is not None and old != payload and previous['files'].get(name) != digest(old):
            raise ValueError('A locally modified skill is preserved: ' + str(path))
        writes.append((path, payload, old))
    receipt = {'format': 'cycode-skill/1', 'files': {n: digest(b) for n, b in files.items()}}
    old = bounded(marker) if marker.exists() else None
    writes.append((marker, (json.dumps(receipt, indent=2) + '\n').encode(), old))
    return target, writes


def install_skill(home, destination):
    # Preflight all files before changing any; unknown files are never removed.
    target, writes = prepare_skill(home, destination)
    for path, payload, old in writes:
        if old != payload:
            atomic(path, payload, old)
    return str(target)


def configure(home, workspace, interpreter, skill_home, access='read', enabled=False, state_dir=None, config_path=None):
    home = ordinary(home).resolve(strict=True)
    workspace = ordinary(workspace).resolve(strict=True)
    if not workspace.is_dir():
        raise ValueError('Workspace must be an existing project directory')
    validate_home(home, enabled)
    interpreter = ordinary(interpreter).resolve(strict=True)
    if not interpreter.is_file():
        raise ValueError('Missing private interpreter')
    config = ordinary(config_path or workspace / '.codex/config.toml')
    key = digest((str(workspace) + '\n' + str(config)).encode('utf-8'))[:20]
    state = ordinary(state_dir or home / 'agent-state') / key
    receipt_file = state / 'connection.json'
    previous = json.loads(bounded(receipt_file)) if receipt_file.exists() else {}
    before = bounded(config) if config.exists() else None
    # Versioned payloads must not start a second SDK runtime in the live old data root.
    current_state = ordinary(home / 'agent-state') / key
    current_receipt = current_state / 'connection.json'
    current_before = bounded(current_receipt) if current_receipt.exists() else None
    if current_receipt != receipt_file and current_before is not None:
        owned = json.loads(current_before)
        if (owned.get('format') != 'cycode-agent-connection/1' or
                owned.get('workspace') != str(workspace) or owned.get('config') != str(config)):
            raise ValueError('Unowned registration state preserved: ' + str(current_receipt))
    block = block_for(home, workspace, interpreter, current_state / 'jobs', access, enabled)
    updated = managed_config(before, block, previous.get('managedBlockSha256'))
    # Refuse conflicts before copying the skill or touching project settings.
    skill = install_skill(home, ordinary(skill_home))
    if before != updated:
        if before is not None:
            backup = state / ('config-before-' + digest(before) + '.toml')
            if not backup.exists():
                atomic(backup, before, None)
        atomic(config, updated, before)
    receipt = {'format': 'cycode-agent-connection/1', 'workspace': str(workspace),
               'home': str(home), 'config': str(config), 'skill': skill,
               'enabled': enabled, 'access': access, 'nativePresent': (home / 'cycode_cli.exe').is_file(),
               'managedBlockSha256': digest(block.encode('utf-8')), 'executed': False}
    old = bounded(receipt_file) if receipt_file.exists() else None
    payload = (json.dumps(receipt, indent=2) + '\n').encode('utf-8')
    atomic(receipt_file, payload, old)
    if current_receipt != receipt_file:
        # Future ordinary updates can use this new home's default receipt.
        atomic(current_receipt, payload, current_before)
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path)
    parser.add_argument('--home', type=Path, default=ROOT)
    parser.add_argument('--python', type=Path)
    parser.add_argument('--skill-home', type=Path, default=Path(os.environ.get('CODEX_HOME', Path.home() / '.codex')) / 'skills')
    parser.add_argument('--state-dir', type=Path, help='Existing registration-state root when switching to a versioned installation; preserves ownership checks')
    parser.add_argument('--config', type=Path, help='Explicit client config; default is project-scoped .codex/config.toml')
    parser.add_argument('--access', choices=('read', 'edit'), default='read')
    parser.add_argument('--enable', action='store_true')
    parser.add_argument('--skill-only', action='store_true')
    parser.add_argument('--status', action='store_true')
    args = parser.parse_args(argv)
    if args.skill_only:
        if args.enable or args.workspace or args.status:
            parser.error('--skill-only cannot register/enable a connection')
        print(json.dumps({'skill': install_skill(args.home, args.skill_home), 'executed': False}))
        return 0
    if not args.workspace:
        parser.error('--workspace is required to register or inspect a connection')
    if args.status:
        config = args.config or args.workspace / '.codex/config.toml'
        parsed = tomllib.loads(bounded(config).decode('utf-8-sig')) if config.exists() else {}
        server = parsed.get('mcp_servers', {}).get(SERVER)
        print(json.dumps({'registered': server is not None, 'enabled': bool(server and server.get('enabled', True)),
                         'nativePresent': (args.home / 'cycode_cli.exe').is_file(), 'executed': False}))
        return 0
    result = configure(args.home, args.workspace, args.python or args.home / 'tools/python/python.exe',
                       args.skill_home, args.access, args.enable, state_dir=args.state_dir, config_path=args.config)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(2)

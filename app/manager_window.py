"""Compatibility entry point; graphical controls are owned by the CyUI host."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import time
import core_agents as agents
import core_dependencies as dependencies
import core_updates as updates


def run(home, argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--home', type=Path, default=home)
    parser.add_argument('--installation-root', type=Path)
    parser.add_argument('--state', type=Path)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--language', choices=('en', 'fr'))
    parser.add_argument('--page', choices=('agents', 'dependencies', 'updates'))
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--check', action='store_true')
    modes.add_argument('--list', action='store_true')
    modes.add_argument('--dependencies', action='store_true')
    modes.add_argument('--background', action='store_true')
    args = parser.parse_args(argv)
    manager = agents.Manager(args.home, root=args.installation_root, state=args.state)
    from manager_backend import Backend
    language = args.language or Backend(manager).preferences().get('language', 'en')
    cache = args.cache or updates.cache_directory() / agents.shared.digest(str(manager.root).encode())[:20]
    if args.check:
        print(json.dumps(updates.check(manager.home, cache=cache, force=True), ensure_ascii=False))
        return 0
    if args.list:
        print(json.dumps({'clients': {key: c.name for key, c in agents.CLIENTS.items()},
                          'connections': manager.receipts()}, ensure_ascii=False, indent=2))
        return 0
    if args.dependencies:
        print(json.dumps(dependencies.inspect(manager.home, root=manager.root), ensure_ascii=False, indent=2))
        return 0
    if os.name != 'nt':
        raise RuntimeError('The CyUI Manager desktop host currently requires Windows')
    if args.background:
        try:
            found = updates.check(manager.home, cache=cache)
            if found['status'] != 'available': return 0
            previous = updates.read_state(cache / 'offered')
            if previous.get('version') == found['release']['version'] and 0 <= time.time() - previous.get('at', 0) < updates.CHECK_INTERVAL_SECONDS:
                return 0
            updates.atomic_json(cache / 'offered/state.json', {'version': found['release']['version'], 'at': time.time()})
            message = ('CyCodeCore {version} est disponible. Ouvrir le gestionnaire ?' if language == 'fr'
                       else 'CyCodeCore {version} is available. Open the manager?').format(version=found['release']['version'])
            if ctypes.windll.user32.MessageBoxW(None, message, 'CyCodeCore Manager', 0x24) != 6: return 0
            args.page = 'updates'
        except Exception:
            return 0
    executable = manager.home / 'CyCodeManager.exe'
    if not executable.is_file():
        raise ValueError('CyCodeManager.exe is missing. Build/install the CyUI Manager; no legacy UI is substituted.')
    command = [str(executable), '--home', str(manager.home), '--cache', str(cache)]
    if args.language: command += ['--language', args.language]
    if args.page: command += ['--page', args.page]
    if args.installation_root: command += ['--installation-root', str(manager.root)]
    if args.state: command += ['--state', str(args.state)]
    subprocess.Popen(command, close_fds=True, creationflags=subprocess.CREATE_NO_WINDOW)
    return 0

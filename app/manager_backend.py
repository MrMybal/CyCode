"""One reviewed Manager action using the existing services, independent of UI."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import uuid
import core_agents as agents
import core_dependencies as dependencies
import core_updates as updates
from core_installation import manifest
from scripts.install_core_agents import ordinary, bounded, atomic


def plan_digest(plan):
    rows = [(str(path), hashlib.sha256(after).hexdigest() if after is not None else None,
             hashlib.sha256(before).hexdigest() if before is not None else None)
            for path, (after, before) in sorted(plan.writes.items())]
    return hashlib.sha256(json.dumps(rows, ensure_ascii=False).encode()).hexdigest()


class Backend:
    def __init__(self, manager, cache=None):
        self.manager = manager
        self.cache = cache or updates.cache_directory() / agents.shared.digest(str(manager.root).encode())[:20]

    def preferences(self):
        path = ordinary(self.manager.state / 'preferences.json')
        return json.loads(bounded(path)) if path.exists() else {}

    def prepare(self, request):
        action = request['action']
        if action == 'agents.prepare':
            return self.manager.prepare(request['clients'], Path(request['project']) if request.get('project') else None,
                scope=request.get('scope', 'project'), access=request.get('access', 'read'),
                skill=request.get('skill', True), mcp=request.get('mcp', True),
                mcp_profile=request.get('mcpProfile', 'code'))
        if action == 'agents.refresh':
            return self.manager.prepare_refresh()
        if action == 'agents.remove':
            return self.manager.prepare_remove(request['id'])
        raise ValueError('Unknown reviewed action')

    def call(self, request, *, cancelled=lambda: False, progress=None):
        action, manager = request['action'], self.manager
        if action == 'snapshot':
            distribution = manifest(manager.home)[0]
            return {'version': distribution['version'], 'localSourceChanges': distribution.get('localSourceChanges', False),
                'home': str(manager.home), 'root': str(manager.root),
                'clients': [{'id': k, 'name': c.name} for k, c in agents.CLIENTS.items()],
                'connections': manager.receipts(), 'dependencies': dependencies.inspect(manager.home, root=manager.root),
                'preferences': self.preferences()}
        if action == 'preferences.save':
            preferences = request['preferences']
            if (set(preferences) != {'language', 'theme', 'compact'} or preferences['language'] not in ('en', 'fr')
                    or preferences['theme'] not in ('navy', 'mid-grey', 'light-grey', 'amber-night', 'violet')
                    or type(preferences['compact']) is not bool):
                raise ValueError('Invalid Manager preferences')
            path = ordinary(manager.state / 'preferences.json')
            with updates_lock(manager.state):
                previous = bounded(path) if path.exists() else None
                value = json.loads(previous) if previous else {}
                value.update(preferences)
                atomic(path, json.dumps(value).encode(), previous)
            return preferences
        if action in ('agents.prepare', 'agents.refresh', 'agents.remove'):
            plan = self.prepare(request)
            token = uuid.uuid4().hex
            path = manager.state / 'plans' / (token + '.json')
            atomic(path, json.dumps({'format': 'cycode-manager-plan/1', 'created': time.time(),
                'request': request, 'fingerprint': plan_digest(plan)}).encode(), None)
            return {'plan': token, **plan.preview()}
        if action == 'agents.apply':
            token = request['plan']
            if not isinstance(token, str) or not re.fullmatch('[a-f0-9]{32}', token):
                raise ValueError('Expected an issued plan')
            path = ordinary(manager.state / 'plans' / (token + '.json'))
            with updates_lock(manager.state):
                data = json.loads(bounded(path))
                if data.get('format') != 'cycode-manager-plan/1' or not 0 <= time.time() - data['created'] <= 1800:
                    raise ValueError('Review expired; prepare the changes again')
                plan = self.prepare(data['request'])
                if plan_digest(plan) != data['fingerprint']:
                    raise ValueError('Configuration changed after preview; prepare again')
                result = manager.apply(plan)
                path.unlink()
                return result
        if action == 'dependencies.install':
            return dependencies.install(manager.home, root=manager.root, accepted=request.get('accepted') is True,
                cancelled=cancelled, progress=progress)
        if action == 'updates.check':
            return updates.check(manager.home, cache=self.cache, force=True)
        if action == 'updates.install':
            # Recheck the actual offered release; do not accept arbitrary download URLs.
            offer = updates.check(manager.home, cache=self.cache, force=True)['release']
            if not offer or offer['version'] != request.get('version') or request.get('accepted') is not True:
                raise ValueError('Release changed or installation was not confirmed')
            return updates.install_update(manager.home, offer, cache=self.cache, language=request.get('language', 'en'),
                refresh=request.get('refresh', True), cancelled=cancelled, progress=progress, manager_state=manager.state)
        raise ValueError('Unknown Manager action')


def updates_lock(state):
    # A distinct issuance lock serializes apply of the same token; Manager.apply
    # keeps its existing connection/configuration lock and conflict checks.
    from core_installation import management_lock
    return management_lock(state / 'plans')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--home', type=Path, required=True)
    parser.add_argument('--installation-root', type=Path)
    parser.add_argument('--state', type=Path)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--cancel-file', type=Path)
    parser.add_argument('--request', required=True)
    options = parser.parse_args(argv)
    if len(options.request) > 65536:
        raise ValueError('Manager request is too large')
    manager = agents.Manager(options.home, root=options.installation_root, state=options.state)
    def progress(value):
        print(json.dumps({'event': 'progress', 'value': value}, ensure_ascii=False), flush=True)
    try:
        result = Backend(manager, cache=options.cache).call(json.loads(options.request),
            cancelled=lambda: options.cancel_file is not None and options.cancel_file.exists(), progress=progress)
        print(json.dumps({'event': 'result', 'value': result}, ensure_ascii=False), flush=True)
        return 0
    except Exception as error:
        print(json.dumps({'event': 'error', 'message': str(error)}, ensure_ascii=False), flush=True)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

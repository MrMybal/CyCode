"""Explicit production Core/CLI/MCP profile acceptance on a new private fixture."""
import argparse
import asyncio
import json
import os
from pathlib import Path
import subprocess
import time
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    args = parser.parse_args()
    exe = args.executable.resolve(strict=True)
    root = args.fixture.absolute()
    root.mkdir(parents=True, exist_ok=False)
    workspace = root / 'project'
    workspace.mkdir()
    source = workspace / 'example.txt'
    source.write_bytes('original été\n'.encode())
    env = {k: v for k, v in os.environ.items() if k not in
           ('CYCODE_PERMISSIONS', 'CYCODE_MCP_ACCESS_PROFILE', 'CYMCP_TOKEN', 'CYTOOLS_TOKEN')}
    env.update(CYCODE_CORE_HOME=str(root / 'home'), CYCODE_INDEX_DIR=str(root / 'index'), CYCODE_INDEX_AT_START='0')
    checks = []
    processes, logs = [], []

    def start(name, profile=None, state=None):
        descriptor = root / (name + '.json')
        log = (root / (name + '.log')).open('wb')
        logs.append(log)
        argv = [str(exe), '--sdk', '--workspace', str(workspace), '--disable-plugins',
                '--data-dir', str(state or root / (name + '-state')), '--service-descriptor', str(descriptor)]
        if profile is not None:
            argv += ['--local-access-profile', profile]
        process = subprocess.Popen(argv + ['serve'], stdin=subprocess.PIPE, stdout=log, stderr=log, env=env)
        processes.append(process)
        deadline = time.monotonic() + 40
        while not descriptor.is_file():
            assert process.poll() is None and time.monotonic() < deadline, name + ' not ready'
            time.sleep(.05)
        return process, json.loads(descriptor.read_text('utf-8'))

    def stop(process):
        process.terminate()
        process.wait(timeout=20)

    def request(d, method, params, role='read', credential=None, principal=None):
        headers = {'Content-Type': 'application/json'}
        if credential is not None:
            headers['Authorization'] = 'Bearer ' + credential
        body = {'protocolVersion': '1.0', 'id': 1, 'method': method, 'params': params}
        if principal:
            body['principal'] = principal
        req = urllib.request.Request(d[role + 'Url'], json.dumps(body).encode(), headers)
        try:
            with urllib.request.urlopen(req, timeout=30) as response:
                return response.status, json.load(response)
        except urllib.error.HTTPError as error:
            return error.code, None

    def domain(d, op, parameters, **kw):
        return request(d, 'Invoke', {'operationId': op, 'parameters': parameters}, **kw)

    def cli(*argv, expected=0):
        result = subprocess.run([str(exe), *argv], env=env, capture_output=True, text=True, encoding='utf-8', timeout=40)
        assert result.returncode == expected, (argv, result.stderr[:1500])
        return json.loads(result.stdout if expected == 0 else result.stderr)

    async def real_mcp(d):
        from mcp import ClientSession
        from mcp.client.streamable_http import streamablehttp_client
        # Same URL-only transport used by chat clients; no headers or token.
        async with streamablehttp_client(d['mcpReadUrl']) as (read, write, _):
            async with ClientSession(read, write) as session:
                await session.initialize()
                assert len((await session.list_tools()).tools) == 5

                async def call(name, arguments, error=False):
                    reply = await session.call_tool('cy_tools_call', {'name': name, 'arguments': arguments})
                    assert bool(reply.isError) == error, reply
                    return json.loads(reply.content[0].text)

                live = await call('documents_read', {'path': 'example.txt'})
                edited = await call('documents_replace', {'path': 'example.txt', 'expectedRevision': live['revision'],
                                                          'text': 'MCP sans jeton été\n'})
                await call('documents_save', {'path': 'example.txt', 'expectedRevision': edited['revision']})
                assert source.read_bytes() == 'MCP sans jeton été\n'.encode()
                await call('documents_read', {'path': 42}, True)
                await call('documents_replace', {'path': 'example.txt', 'expectedRevision': live['revision'], 'text': 'stale'}, True)
                await call('documents_read', {'path': '../outside.txt'}, True)

    try:
        free, d = start('default')
        assert d['localAccessProfile'] == 'free' and not d['requireToken'] and not d['controlOperations']
        assert d['readToken'] == d['editToken'] == '' and d['url'].startswith('http://127.0.0.1:')
        manifest = request(d, 'DescribeTool', {})[1]['result']
        lock = json.loads((Path(__file__).resolve().parents[1] / 'cymcp-source.lock.json').read_text('utf-8'))
        assert manifest['sdk']['revision'] == lock['revision']
        assert manifest['sdk']['version'] == lock['version']
        assert len(manifest['operations']) == 257
        asyncio.run(real_mcp(d))
        checks.append('free-url-only-official-mcp-read-replace-save-with-real-engine')
        checks.append('free-keeps-schema-revision-and-workspace-validation')
        base = ['--sdk', '--connect', d['mcpReadUrl'], '--data-dir', str(root / 'cli-proxy')]
        read = cli(*base, 'invoke', 'documents_read', '--params', '{"path":"example.txt"}')
        replaced = cli(*base, 'invoke', 'documents_replace', '--params', json.dumps(
            {'path': 'example.txt', 'expectedRevision': read['revision'], 'text': 'CLI URL only\n'}))
        assert replaced['text'] == 'CLI URL only\n'
        checks.append('cli-connect-mcp-address-only-read-and-modification')
        # Legacy native command shape is still independent of SDK presentation.
        assert 'text' in cli('--disable-plugins', str(workspace), 'documents.read', '{"path":"example.txt"}')
        checks.append('legacy-native-cli')
        protected, p = start('protected', 'protected')
        assert p['requireToken'] and p['controlOperations'] and p['readToken']
        assert request(p, 'DescribeTool', {})[0] == 401
        assert request(p, 'DescribeTool', {}, credential='invalid')[0] == 401
        live = domain(p, 'documents_read', {'path': 'example.txt'}, credential=p['readToken'])[1]['result']
        update = {'path': 'example.txt', 'expectedRevision': live['revision'], 'text': 'controlled\n'}
        denied = domain(p, 'documents_replace', update, credential=p['readToken'], principal='local-owner')[1]
        assert denied['error']['code'] == 'AccessDenied'
        assert domain(p, 'documents_replace', update, role='edit', credential=p['editToken'])[1]['result']['text'] == update['text']
        denied_cli = cli('--sdk', '--connect', p['url'], '--data-dir', str(root / 'protected-proxy'),
                         'invoke', 'documents_read', '--params', '{"path":"example.txt"}', expected=1)
        assert denied_cli['error']['code'] == 'AuthenticationRequired'
        checks.append('protected-token-required-and-controlled-roles-preserved')
        stop(protected)
        resumed, saved = start('protected-resumed', state=root / 'protected-state')
        assert saved['localAccessProfile'] == 'protected' and saved['requireToken']
        stop(resumed)
        checks.append('explicit-saved-profile-preserved-on-restart')
        controlled, c = start('controlled', 'local-controlled')
        assert not c['requireToken'] and c['controlOperations']
        live = domain(c, 'documents_read', {'path': 'example.txt'})[1]['result']
        assert domain(c, 'documents_replace', {'path': 'example.txt', 'text': 'denied',
                       'expectedRevision': live['revision']})[1]['error']['code'] == 'AccessDenied'
        stop(controlled)
        checks.append('local-controlled-url-only-reads-and-refuses-ungranted-writes')
        token_only, t = start('token-only', 'token-only')
        assert t['requireToken'] and not t['controlOperations']
        assert request(t, 'DescribeTool', {})[0] == 401
        live = domain(t, 'documents_read', {'path': 'example.txt'}, credential=t['readToken'])[1]['result']
        assert 'result' in domain(t, 'documents_replace', {'path': 'example.txt', 'text': 'token write\n',
                                 'expectedRevision': live['revision']}, credential=t['readToken'])[1]
        stop(token_only)
        checks.append('token-only-requires-token-and-permits-mutations-with-read-token')
        # A persisted file without a selected profile migrates to Free.
        unset_state = root / 'unset-state'
        unset_state.mkdir()
        (unset_state / 'local-access.json').write_text('{"format":"cycode-local-access/1"}', encoding='utf-8')
        unset, u = start('unset', state=unset_state)
        assert u['localAccessProfile'] == 'free' and not u['requireToken']
        stop(unset)
        checks.append('unset-settings-migrate-to-free')
        result = {'sdkRevision': manifest['sdk']['revision'], 'platform': os.name, 'checks': checks,
                  'actionsUsed': False, 'fixtureExecutablesBuilt': False, 'nativeChatInCore': False}
        (root / 'report.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(result, indent=2))
    finally:
        for process in processes:
            if process.poll() is None:
                stop(process)
        for log in logs:
            log.close()


if __name__ == '__main__':
    main()

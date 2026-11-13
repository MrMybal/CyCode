"""Official SDK discovery/guards with Python handlers; no native process starts."""
import asyncio
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from mcp_profile import ProfileClient, CODE_OPERATIONS, GATEWAY, client_profile
from cytools_core import Runtime, CyToolError
from cytools_core.transports.api import LocalClient
from cytools_core.transports.mcp import create_server
from mcp.types import ListToolsRequest

DESCRIPTOR = json.loads((APP / 'CyTool.json').read_text('utf-8'))
SYSTEM = {'os': 'Windows', 'architecture': 'x64', 'osVersion': 'test', 'cpuThreads': 4,
          'ramFreeBytes': 8 * 1024**3, 'diskFreeBytes': 8 * 1024**3, 'gpus': [], 'computeBackends': ['CPU']}
DOCUMENT = {'path': 'main.cpp', 'text': 'int value;', 'revision': 1, 'dirty': False,
            'lineEnding': 'LF', 'encoding': 'UTF-8'}


class Profiles(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='cycode-profile-data-')
        self.addCleanup(self.directory.cleanup)
        self.runtime = Runtime(copy.deepcopy(DESCRIPTOR), self.directory.name, system=SYSTEM)
        self.addCleanup(self.runtime.close)
        def unused(context, parameters):
            raise AssertionError('This process-free fixture must not invoke another operation')
        for operation in DESCRIPTOR['operations']:
            self.runtime.register(operation['id'], (lambda context, parameters: DOCUMENT)
                if operation['id'] in ('documents_read', 'documents_replace') else unused)
        self.runtime.start()
        self.principal = self.runtime.authenticate(self.runtime.create_client('reader')['token'])
        self.local = LocalClient(self.runtime, self.principal)
        self.compact = ProfileClient(self.local)
        self.session = self.compact.call('CreateSession', temporary=True)['sessionId']

    def invoke(self, operation, parameters, **extra):
        return self.compact.call('SubmitJob', sessionId=self.session, operationId=GATEWAY,
            parameters={'operationId': operation, 'parameters': parameters}, **extra)

    def test_input_schemas_and_on_demand_descriptors_remain_canonical(self):
        presented = {o['id']: o for o in self.compact.call('DescribeTool')['operations']}
        self.assertEqual(set(presented), CODE_OPERATIONS | {GATEWAY})
        for op in DESCRIPTOR['operations']:
            self.assertEqual(self.compact.call('DescribeOperation', operationId=op['id']), op)
            if op['id'] in presented:
                self.assertEqual(presented[op['id']]['inputSchema'], op['inputSchema'])
        self.assertNotIn('inputSchema', self.compact.call('ListOperations')[0])
        self.assertIs(client_profile(self.local, 'full'), self.local)
        self.assertEqual(ProfileClient(self.local, 'full').call('DescribeTool'), DESCRIPTOR)

    def test_gateway_uses_real_sdk_validation_and_permissions(self):
        for operation, parameters in [('documents_replace', {'path': 'main.cpp', 'text': 'changed', 'expectedRevision': 1}),
                                      ('documents_read', {'path': 123})]:
            with self.subTest(operation=operation), self.assertRaises(CyToolError):
                self.invoke(operation, parameters)
        job = self.invoke('documents_read', {'path': 'main.cpp'})
        result = self.runtime.wait(self.principal, job['jobId'], timeout=5)
        self.assertEqual(result['state'], 'Completed', result)
        self.assertEqual(result['operationId'], 'documents_read')
        self.assertEqual(result['outputs'], DOCUMENT)
        stranger = self.runtime.authenticate(self.runtime.create_client('other')['token'])
        with self.assertRaises(CyToolError):
            LocalClient(self.runtime, stranger).call('GetJob', jobId=job['jobId'])

    def test_invalid_gateway_envelopes_are_refused(self):
        for envelope in (None, {}, {'operationId': [], 'parameters': {}},
                         {'operationId': 'unknown', 'parameters': {}},
                         {'operationId': 'documents_read', 'parameters': [], 'extra': True}):
            with self.subTest(envelope=envelope), self.assertRaises(CyToolError):
                self.compact.call('SubmitJob', sessionId=self.session, operationId=GATEWAY, parameters=envelope)

    def test_gateway_can_run_and_estimate_delegate_canonical_constraints(self):
        parameters = {'operationId': 'documents_read', 'parameters': {'path': 'main.cpp'}}
        for method in ('CanRun', 'EstimateResources'):
            self.assertEqual(self.compact.call(method, operationId=GATEWAY, parameters=parameters),
                self.local.call(method, **parameters))

    def test_official_adapter_advertises_a_smaller_catalogue(self):
        async def discover(client):
            server = create_server(client)
            response = await server.request_handlers[ListToolsRequest](ListToolsRequest(method='tools/list'))
            return response.root.tools
        full = asyncio.run(discover(self.local))
        compact = asyncio.run(discover(self.compact))
        size = lambda tools: len(json.dumps([t.model_dump(exclude_none=True) for t in tools], ensure_ascii=False).encode('utf-8'))
        metrics = {'fullTools': len(full), 'compactTools': len(compact), 'fullBytes': size(full), 'compactBytes': size(compact)}
        print('MCP_METADATA ' + json.dumps(metrics))
        self.assertEqual(len(full), 278)
        self.assertEqual(len(compact), 38)
        self.assertLess(size(compact), size(full) / 4)
        self.assertIn('op_core_invoke', {t.name for t in compact})


if __name__ == '__main__':
    unittest.main()

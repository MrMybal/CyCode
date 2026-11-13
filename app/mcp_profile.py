"""Presentation profiles for the official CyToolsCore MCP adapter.

Canonical schemas, permissions, resource reservations and jobs stay in the SDK.
The compact profile advertises common code operations plus one guarded gateway
to every other canonical operation; it does not maintain another scheduler.
"""
from __future__ import annotations
import copy

CODE_OPERATIONS = frozenset('''service_status language_status workspace_files search_text
    documents_read documents_read_page documents_read_lines language_definition language_references
    language_hover language_document_symbols language_symbols language_symbol_resolve
    analysis_prepare analysis_status analysis_results'''.split())
GATEWAY = 'core_invoke'


class ProfileClient:
    def __init__(self, client, profile='code'):
        if profile not in ('code', 'full'):
            raise ValueError('Unknown MCP profile')
        self.client, self.profile = client, profile
        self.canonical = client.call('DescribeTool')
        self.operations = {o['id']: o for o in self.canonical['operations']}

    def _present(self, op):
        value = copy.deepcopy(op)
        # Full output/constraint documentation remains available on demand via
        # cy_describe_operation. MCP domain tools return SDK job envelopes.
        value['outputSchema'] = {'type': 'object'}
        value['description'] = op['description'].split('. ')[0][:220]
        return value

    def _gateway(self):
        return {'id': GATEWAY, 'name': 'Invoke another Core operation',
            'description': 'Invoke a canonical operation discovered once with cy_list_operations, then cy_describe_operation. Actual validation, permissions and jobs are enforced by CyToolsCore.',
            'inputSchema': {'type': 'object', 'properties': {
                'operationId': {'type': 'string', 'minLength': 1}, 'parameters': {'type': 'object'}},
                'required': ['operationId', 'parameters'], 'additionalProperties': False},
            'outputSchema': {'type': 'object'}, 'permissions': [], 'supportedBackends': [],
            'resourceRequirements': {}, 'canRunAsync': True, 'canCancel': True, 'supportsProgress': False}

    def call(self, method, **parameters):
        if self.profile == 'full':
            return self.client.call(method, **parameters)
        if method == 'DescribeTool':
            value = copy.deepcopy(self.canonical)
            value['operations'] = [self._present(o) for o in self.operations.values() if o['id'] in CODE_OPERATIONS]
            value['operations'].append(self._gateway())
            return value
        if method == 'ListOperations':
            # One inexpensive index; exact input/output schemas are fetched for
            # the selected operation, not 257 times for every catalogue read.
            return [{'id': o['id'], 'name': o['name'], 'description': o['description'].split('. ')[0][:120],
                     'permissions': o.get('permissions', [])} for o in self.operations.values()]
        if method == 'DescribeOperation' and parameters.get('operationId') == GATEWAY:
            return self._gateway()
        if method in ('SubmitJob', 'CanRun', 'EstimateResources') and parameters.get('operationId') == GATEWAY:
            from cytools_core import CyToolError
            envelope = parameters.get('parameters')
            if (not isinstance(envelope, dict) or set(envelope) != {'operationId', 'parameters'}
                    or not isinstance(envelope['operationId'], str)
                    or envelope['operationId'] not in self.operations or not isinstance(envelope['parameters'], dict)):
                raise CyToolError('InvalidParameters', 'Choose a canonical operation and its exact parameters')
            parameters = {**parameters, 'operationId': envelope['operationId'], 'parameters': envelope['parameters']}
        return self.client.call(method, **parameters)


def client_profile(client, profile):
    return client if profile == 'full' else ProfileClient(client, profile)

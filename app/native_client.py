"""Compatibility IPC client; production MCP and CLI use CyMCPSDK natively."""
import json
import os
from pathlib import Path
import socket
import time
import uuid
from native_sdk import NativeSDKError as CyToolError
from contracts import GIT_METHODS, PAGED_METHODS

class NativeClient:
    def __init__(self, descriptor):
        self.descriptor = Path(descriptor)

    def call(self, method, parameters, cancellation=None, progress=None):
        try:
            project_operation=method.startswith('project.') and method not in ('project.inspect','project.cancel','project.status','project.output')
            test_operation=method in ('tests.discover','tests.run')
            java_reload=method == 'debug.hotReload'
            analysis_operation=method in ('analysis.prepare','language.rename')
            build_operation=method in ('analysis.inspect','build.configure','build.run','build.presetRun','msbuild.build','msbuild.rebuild',
                'msbuild.clean','msbuild.restore','msbuild.evaluate','msbuild.prepare',
                'unreal.generate','unreal.database','unreal.build','unreal.prepare','unreal.automationRun')
            # Bundled VCS providers may validate identity/mapping, inspect local
            # changes and then run a separate bounded network operation.
            wait_seconds = 86460 if method.startswith('packages.') else 1200 if method.startswith('vcs.replay.') else 600 if method == 'vcs.providerAction' or method.startswith('vcs.merge.') or method.startswith('vcs.checkout.') or method.startswith('vcs.conflict.') or method.startswith('vcs.worktree.') or method.startswith('vcs.submodule') else 200
            if analysis_operation or method in PAGED_METHODS or method in ('changes.apply','changes.undo','changes.historyPrepare'):
                wait_seconds=86460
            if build_operation or project_operation:
                timeout = parameters.get('timeoutSeconds', 150 if method in ('msbuild.build','msbuild.rebuild','msbuild.clean','msbuild.restore') else 180)
                if type(timeout) is not int or not 1 <= timeout <= 86400:
                    raise ValueError('Invalid native build timeout')
                # A saved project profile can own its deadline when no override
                # is supplied; wait for that owner rather than releasing a live job.
                wait_seconds = max(200, timeout + 60) if not project_operation or 'timeoutSeconds' in parameters else 86460
            operation_id=(parameters.get('operationId') or uuid.uuid4().hex) if java_reload else uuid.uuid4().hex if project_operation or test_operation or build_operation or analysis_operation else None
            if operation_id:
                parameters={**parameters,'operationId':operation_id}
            def stop_if_cancelled():
                if not cancellation or not operation_id:
                    return
                try:
                    cancellation.check()
                except Exception:
                    # Match the executing call; a queued cancelled job must not
                    # stop a different actor's build in the shared native service.
                    try:
                        self.call('analysis.cancel' if analysis_operation else 'debug.cancelReload' if java_reload else 'tests.cancel' if test_operation else 'build.cancel' if build_operation else 'project.cancel',{'operationId':operation_id})
                    except Exception:
                        # A failed cancellation notification must not release the
                        # owning SDK reservation while its native call is alive.
                        # Keep receiving, and retry at the next socket timeout.
                        pass
            endpoint = json.loads(self.descriptor.read_text('utf-8'))
            if endpoint['protocol'] != 'cycode-native/1' or endpoint['host'] != '127.0.0.1':
                raise ValueError('Unsupported native endpoint')
            request = {'token': endpoint['token'], 'method': method, 'params': parameters}
            with socket.create_connection(('127.0.0.1', endpoint['port']), timeout=5) as connection:
                connection.settimeout(1)
                connection.sendall((json.dumps(request, ensure_ascii=False) + '\n').encode('utf-8'))
                data = bytearray()
                deadline = time.monotonic() + wait_seconds
                last_progress=None
                progress_error=None
                # Keep the reservation until native work actually stops, even
                # after cancellation. Custom build deadlines also extend this
                # transport wait; the native owner still performs termination.
                while b'\n' not in data:
                    try:
                        chunk = connection.recv(65536)
                    except socket.timeout:
                        stop_if_cancelled()
                        if analysis_operation and progress:
                            try:
                                state=self.call('analysis.status',{'operationId':operation_id})
                                signature=(state['phase'],state['completed'],state['total'])
                                if signature!=last_progress:
                                    last_progress=signature
                                    progress(state)
                            except Exception as error:
                                # A cancelled SDK progress callback can itself
                                # raise. Keep the job reservation until this
                                # native operation acknowledges/stops.
                                progress_error=error
                        if not analysis_operation and time.monotonic() > deadline:
                            raise OSError('Native operation did not stop within its deadline')
                        continue
                    if not chunk:
                        raise OSError('The native Core disconnected')
                    data.extend(chunk)
                    if len(data) > 20 * 1024 * 1024:
                        raise ValueError('Native response exceeds 20 MiB')
                response = json.loads(data)
            if cancellation:
                try:
                    cancellation.check()
                except Exception:
                    terminal=response.get('result',{}).get('terminal',{}) if project_operation else {}
                    if terminal.get('id'):
                        self.call('terminal.close',{'id':terminal['id']})
                    raise
            if 'error' in response:
                failure=response['error']
                raise CyToolError(failure.get('code','NativeOperationFailed'), failure['message'],
                                  details=failure.get('details'),recoverable=failure.get('code') not in ('InvalidParams','InvalidRequest','UnsupportedProtocol'))
            if progress_error is not None:
                raise progress_error
            return response['result']
        except CyToolError:
            raise
        except (OSError, ValueError, KeyError) as error:
            raise CyToolError('NativeUnavailable', str(error), recoverable=True) from error

def native_parameters(method, parameters):
    if method in GIT_METHODS:
        if parameters.get('provider', 'git') != 'git':
            raise CyToolError('UnsupportedProvider', 'Use vcs_provider_action for external providers.', recoverable=True)
        # Typed Git schemas/permissions do not authorize arbitrary native DLL
        # callbacks or their network processes. Keep the common Engine service,
        # selecting its built-in provider and workspace repository explicitly.
        return {**parameters, 'provider':'git', 'contextPath':parameters.get('repository','.')}
    return parameters

"""Official CyToolsCore SDK/MCP facade over one persistent native CyCode Core."""
import asyncio
import argparse
import json
import os
from pathlib import Path
import socket
import sys
import time
import uuid
from cytools_core import CyToolError, Runtime, load_manifest
from cytools_core.transports.api import LocalClient
from cytools_core.transports.cli import run_cli
# An embedded interpreter may pin another installation's app in python._pth.
# Always pair this facade with its own contracts and host, after loading the
# official SDK from the interpreter's pinned runtime.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from contracts import METHODS, GIT_METHODS, DEBUG_MUTATIONS, DEBUG_EXECUTIONS, PAGED_METHODS
ROOT = Path(__file__).resolve().parents[1]

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


def create_runtime(data_dir, **options):
    runtime = Runtime(load_manifest(Path(__file__).with_name('CyTool.json')), data_dir, **options)
    runtime.configure_inputs(ROOT / 'data' / 'inputs', fields={})
    client = NativeClient(os.environ.get('CYCODE_BRIDGE', str(ROOT / 'data' / 'bridge' / 'native.json')))
    for operation_id, method in METHODS.items():
        def handler(context, parameters, native_method=method):
            context.cancellation.check()
            def report_progress(state):
                context.progress(state['progress'],currentStep=state['phase'],currentStepIndex=state['completed'],stepCount=max(1,state['total']))
            return client.call(native_method, native_parameters(native_method, parameters),
                               None if native_method in PAGED_METHODS or native_method in ('changes.apply','changes.undo','changes.historyPrepare') or native_method.startswith('packages.') or native_method.startswith('vcs.replay.') or native_method.startswith('vcs.merge.') or native_method.startswith('vcs.checkout.') or native_method.startswith('vcs.conflict.') or native_method.startswith('vcs.worktree.') or native_method.startswith('vcs.submodule') or native_method in DEBUG_MUTATIONS or native_method in DEBUG_EXECUTIONS or native_method in ('documents.createUntitled','documents.saveAs','documents.close','debug.dataBreakpoints','debug.runToCursor','debug.restoreRunToCursor','debug.instructionBreakpoints') else context.cancellation,
                               report_progress if native_method in ('analysis.prepare','language.rename') else None)
        runtime.register(operation_id, handler)
    return runtime

def main(argv=None):
    args = list(sys.argv[1:] if argv is None else argv)
    if '--core-workspace' in args:
        from core_host import ManagedCore
        parser=argparse.ArgumentParser(add_help=False)
        parser.add_argument('--core-workspace',type=Path,required=True)
        parser.add_argument('--core-executable',type=Path)
        parser.add_argument('--disable-native-plugins',action='store_true')
        options,remaining=parser.parse_known_args(args)
        directory=Path(remaining[remaining.index('--data-dir')+1]) if '--data-dir' in remaining else ROOT/'data/jobs'
        previous=os.environ.get('CYCODE_BRIDGE')
        try:
            with ManagedCore(options.core_workspace,directory,options.core_executable,options.disable_native_plugins) as core:
                os.environ['CYCODE_BRIDGE']=str(core.descriptor)
                if ('--core-service' in remaining or '--desktop-service' in remaining) and '--native-descriptor' not in remaining:
                    remaining+=['--native-descriptor',str(core.descriptor)]
                return main(remaining)
        finally:
            if previous is None:os.environ.pop('CYCODE_BRIDGE',None)
            else:os.environ['CYCODE_BRIDGE']=previous
    if '--core-service' in args:
        args[args.index('--core-service')]='--desktop-service'
    if '--desktop-service' in args:
        parser = argparse.ArgumentParser()
        parser.add_argument('--desktop-service', action='store_true')
        parser.add_argument('--service-descriptor', type=Path, required=True)
        parser.add_argument('--native-descriptor', type=Path, required=True)
        parser.add_argument('--data-dir', required=True)
        options = parser.parse_args(args)
        os.environ['CYCODE_BRIDGE'] = str(options.native_descriptor)
        runtime = create_runtime(options.data_dir)
        from cytools_core.transports.http import make_server
        server = None
        try:
            runtime.start()
            read = runtime.create_client('CyCode read-only assistant')
            edit = runtime.create_client('CyCode editing assistant', ['workspace.write', 'vcs.write', 'process.execute', 'plugins.execute'])
            server = make_server(runtime, 0)
            server.timeout = .25
            descriptor = {'protocol': 'cycode-core/1', 'url': f'http://127.0.0.1:{server.server_port}/v1',
                          'readToken': read['token'], 'editToken': edit['token']}
            options.service_descriptor.parent.mkdir(parents=True, exist_ok=True)
            options.service_descriptor.write_text(json.dumps(descriptor), encoding='utf-8')
            if os.name != 'nt':
                options.service_descriptor.chmod(0o600)
            while options.native_descriptor.exists():
                server.handle_request()
        finally:
            if server:
                server.server_close()
            runtime.close()
            options.service_descriptor.unlink(missing_ok=True)
        return 0
    if '--bridge-config' in args:
        parser = argparse.ArgumentParser()
        parser.add_argument('--bridge-config', type=Path, required=True)
        parser.add_argument('--access', choices=['read', 'edit'], default='read')
        parser.add_argument('command', choices=['mcp'])
        options = parser.parse_args(args)
        descriptor = json.loads(options.bridge_config.read_text('utf-8'))
        if descriptor['protocol'] != 'cycode-core/1':
            raise SystemExit('Unsupported CyCode Core service descriptor')
        from cytools_core.transports.http import HTTPClient
        from cytools_core.transports.mcp import serve
        asyncio.run(serve(HTTPClient(descriptor['url'], descriptor['editToken' if options.access == 'edit' else 'readToken'])))
        return 0
    if '--data-dir' not in args:
        args = ['--data-dir', str(ROOT / 'data' / 'jobs'), *args]
    if args[-1:] == ['mcp'] and '--connect' not in args:
        directory = args[args.index('--data-dir') + 1]
        runtime = create_runtime(directory)
        try:
            runtime.start()
            permissions = [item.strip() for item in os.environ.get('CYCODE_PERMISSIONS', '').split(',') if item.strip()]
            credentials = runtime.create_client('CyCode MCP', permissions)
            client = LocalClient(runtime, runtime.authenticate(credentials['token']))
            from cytools_core.transports.mcp import serve
            asyncio.run(serve(client))
            return 0
        finally:
            runtime.close()
    return run_cli(create_runtime, args)

if __name__ == '__main__':
    raise SystemExit(main())

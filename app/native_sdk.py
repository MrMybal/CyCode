"""Thin compatibility client to the native CyMCPSDK host. No Python runtime/model."""
from __future__ import annotations
from dataclasses import dataclass, field
import json
import os
from pathlib import Path
import subprocess
import threading
import time
from core_host import find_core


class NativeSDKError(RuntimeError):
    def __init__(self, code, message, *, details=None, recoverable=False):
        super().__init__(message)
        self.code, self.details, self.recoverable = code, details, recoverable

    def to_dict(self):
        return {'code': self.code, 'message': str(self), 'details': self.details, 'recoverable': self.recoverable}


@dataclass(frozen=True)
class Principal:
    client_id: str
    permissions: frozenset[str]
    token: str = field(repr=False)


class NativeRuntime:
    def __init__(self, directory, *, executable=None, native_descriptor=None, workspace=None, **options):
        self.root = Path(directory).resolve()
        self.executable = executable
        self.native_descriptor = native_descriptor or os.environ.get('CYCODE_BRIDGE')
        self.workspace = workspace
        self.options = options
        self.process = None
        self.lock = threading.Lock()
        self.sequence = 0

    def start(self):
        if self.process is not None: return
        self.root.mkdir(parents=True, exist_ok=True)
        from scripts.migrate_legacy_jobs import migrate
        migrate(self.root)
        command = [str(find_core(self.executable)), '--sdk', '--owner', '--data-dir', str(self.root)]
        if self.native_descriptor: command += ['--native-descriptor', str(self.native_descriptor)]
        if self.workspace: command += ['--workspace', str(self.workspace)]
        for source, target in [('ramBytes', '--job-ram-bytes'), ('diskBytes', '--job-disk-bytes')]:
            if source in self.options.get('capacity', {}): command += [target, str(self.options['capacity'][source])]
        command += ['stdio']
        self.errors = (self.root / 'native-automation.stderr.txt').open('w', encoding='utf-8')
        self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.errors,
            text=True, encoding='utf-8', bufsize=1, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))

    def request(self, method, parameters=None, principal=None):
        self.start()
        with self.lock:
            self.sequence += 1
            message = {'protocolVersion': '1.0', 'id': self.sequence, 'method': method, 'params': parameters or {}}
            if principal is not None: message['authToken'] = principal.token
            self.process.stdin.write(json.dumps(message, ensure_ascii=False) + '\n'); self.process.stdin.flush()
            line = self.process.stdout.readline()
            if not line: raise NativeSDKError('NativeUnavailable', 'Native automation host stopped; inspect its private stderr')
            response = json.loads(line)
            if 'error' in response:
                error = response['error']
                raise NativeSDKError(error['code'], error['message'], details=error.get('details'), recoverable=error.get('recoverable', False))
            return response['result']

    def close(self):
        if self.process is None: return
        self.process.stdin.close()
        self.process.wait(timeout=300)
        self.process.stdout.close(); self.errors.close(); self.process = None

    def create_client(self, name='local', permissions=()):
        return self.request('CreateClient', {'name': name, 'permissions': list(permissions)})

    def authenticate(self, token):
        identity = self.request('Authenticate', {'token': token})
        return Principal(identity['clientId'], frozenset(identity['permissions']), token)

    def create_session(self, principal, *, temporary=True, retention_seconds=3600):
        return self.request('CreateSession', {'temporary': temporary, 'retentionSeconds': retention_seconds}, principal)

    def submit(self, principal, session_id, operation_id, parameters, *, backend_id='', model_id='', priority='Normal',
               queue_timeout=None, execution_timeout=None):
        request = {'sessionId': session_id, 'operationId': operation_id, 'parameters': parameters,
                   'backendId': backend_id, 'modelId': model_id, 'priority': priority}
        if queue_timeout is not None: request['queueTimeout'] = queue_timeout
        if execution_timeout is not None: request['executionTimeout'] = execution_timeout
        return self.request('SubmitJob', request, principal)

    def get_job(self, principal, job_id): return self.request('GetJob', {'jobId': job_id}, principal)
    def cancel(self, principal, job_id): return self.request('CancelJob', {'jobId': job_id}, principal)
    def list_jobs(self, principal): return self.request('ListJobs', {}, principal)
    def close_session(self, principal, session_id): return self.request('CloseSession', {'sessionId': session_id}, principal)
    def can_run(self, operation_id, parameters, backend_id='', model_id=''):
        return self.request('CanRun', {'operationId': operation_id, 'parameters': parameters, 'backendId': backend_id, 'modelId': model_id})
    def wait(self, principal, job_id, timeout=30):
        deadline = time.monotonic() + timeout
        while True:
            job = self.get_job(principal, job_id)
            if job['state'] in ('Completed', 'Failed', 'Cancelled'): return job
            if time.monotonic() > deadline: raise NativeSDKError('Timeout', 'Job is still owned by the native service')
            time.sleep(.03)


class LocalClient:
    def __init__(self, runtime, principal): self.runtime, self.principal = runtime, principal
    def call(self, method, **parameters): return self.runtime.request(method, parameters, self.principal)


def create_runtime(directory, **options): return NativeRuntime(directory, **options)


def download_artifact(executable, request, *, cancelled=lambda: False, progress=None):
    """Use CyMCPSDK native HTTPS; Python owns no HTTP downloader or SDK model."""
    import queue
    import tempfile
    destination = Path(request['destination'])
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.download-control-', dir=destination.parent) as temporary:
        cancel = Path(temporary) / 'cancel'
        request = {**request, 'cancelFile': str(cancel)}
        messages = queue.Queue()
        with (Path(temporary)/'stderr.txt').open('w+', encoding='utf-8') as errors:
            process = subprocess.Popen([str(executable), '--sdk-download', json.dumps(request)],
                stdout=subprocess.PIPE, stderr=errors, text=True, encoding='utf-8',
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            def receive():
                try:
                    for line in process.stdout: messages.put(line)
                finally: messages.put(None)
            reader = threading.Thread(target=receive, daemon=True); reader.start()
            result = None
            try:
                while True:
                    if cancelled() and not cancel.exists(): cancel.write_text('cancel', encoding='ascii')
                    try: line = messages.get(timeout=.1)
                    except queue.Empty: continue
                    if line is None: break
                    event = json.loads(line)
                    if event.get('event') == 'progress' and progress: progress(event['value'])
                    elif event.get('event') == 'result': result = event['value']
                status = process.wait(timeout=40)
                if status or result is None:
                    errors.seek(0)
                    detail = errors.read(8192)
                    raise NativeSDKError('DownloadFailed', detail or 'Native artifact download stopped')
                return result
            finally:
                if process.poll() is None:
                    if not cancel.exists(): cancel.write_text('cancel', encoding='ascii')
                    try: process.wait(timeout=40)
                    except subprocess.TimeoutExpired: process.terminate(); process.wait(timeout=10)
                process.stdout.close(); reader.join(timeout=1)

"""Application-owned native host lifecycle; CyToolsCore still owns MCP/jobs/identity."""
import os
import json
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]

def find_core(explicit=None):
    suffix='.exe' if os.name=='nt' else ''
    selected=explicit or os.environ.get('CYCODE_CORE_EXECUTABLE')
    if selected:
        candidate=Path(selected).expanduser().resolve(strict=True)
        if not candidate.is_file():raise ValueError('Expected a trusted native Core executable')
        return candidate
    candidates=[ROOT/('cycode_cli'+suffix),ROOT/'bin'/('cycode_cli'+suffix)]
    for build in ('core','build','core-extraction-build'):
        candidates+=[ROOT/'runtime'/build/'app'/'Release'/('cycode_cli'+suffix),ROOT/'runtime'/build/'app'/('cycode_cli'+suffix)]
    for candidate in candidates:
        if candidate.is_file():return candidate.resolve()
    raise FileNotFoundError('Build CyCode Core or pass --core-executable; no binary is downloaded automatically')

class ManagedCore:
    def __init__(self,workspace,data_dir,executable=None,disable_plugins=False):
        self.workspace=Path(workspace).resolve(strict=True)
        if not self.workspace.is_dir():raise ValueError('Expected a project directory')
        self.executable=find_core(executable)
        self.data=Path(data_dir).resolve()
        self.disable_plugins=disable_plugins
        self.process=None
        self.folder=None
        self.log=None

    def __enter__(self):
        self.data.mkdir(parents=True,exist_ok=True)
        self.folder=tempfile.TemporaryDirectory(prefix='native-core-',dir=self.data)
        self.descriptor=Path(self.folder.name)/'native.json'
        self.log=(Path(self.folder.name)/'native.log').open('wb')
        arguments=[str(self.executable)]
        if self.disable_plugins:arguments+=['--disable-plugins']
        arguments += [str(self.workspace),'--serve',str(self.descriptor)]
        try:
            # A long-lived agent service indexes its project at start (reload if persisted,
            # otherwise a first build) while the agent prepares its first request.
            environment={**os.environ,'CYCODE_INDEX_AT_START':os.environ.get('CYCODE_INDEX_AT_START','1')}
            self.process=subprocess.Popen(arguments,stdin=subprocess.PIPE,stdout=self.log,stderr=subprocess.STDOUT,env=environment,
                cwd=self.workspace,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            deadline=time.monotonic()+45
            while not self.descriptor.is_file():
                if self.process.poll() is not None:raise RuntimeError('Native Core exited before publishing its endpoint')
                if time.monotonic()>deadline:raise TimeoutError('Native Core startup did not publish an endpoint')
                time.sleep(.025)
            return self
        except BaseException:
            self.__exit__(None,None,None)
            raise

    def _preserve_failure(self):
        # Keep bounded local evidence before deleting the authentication
        # descriptor. Do not copy credentials or automatically restart a host.
        directory=self.data/'native-core-failures'/uuid.uuid4().hex
        directory.mkdir(parents=True,exist_ok=False)
        source=Path(self.folder.name)/'native.log'
        if source.is_file():
            with source.open('rb') as stream:
                stream.seek(0,2);size=stream.tell();stream.seek(max(0,size-65536))
                tail=stream.read(65536)
            (directory/'native.log').write_bytes(tail)
        else:size=0
        report={'format':'cycode-native-failure/1','recordedAt':datetime.now(timezone.utc).isoformat(),
            'executable':str(self.executable),'workspace':str(self.workspace),
            'exitCode':self.process.returncode,'logBytes':min(size,65536),'logTruncated':size>65536,
            'automaticRestart':False,'cause':'undetermined'}
        (directory/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        return directory

    def __exit__(self,kind,error,traceback):
        try:
            if self.process:
                if self.process.poll() is None:
                    try:
                        self.process.stdin.write(b'\n');self.process.stdin.flush()
                    except (BrokenPipeError,OSError):pass
                    self.process.stdin.close()
                    try:self.process.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        self.process.terminate()
                        try:self.process.wait(timeout=10)
                        except subprocess.TimeoutExpired:self.process.kill();self.process.wait(timeout=10)
                elif self.process.stdin:self.process.stdin.close()
        finally:
            if self.log:self.log.close()
            if self.process and self.process.returncode not in (None,0) and self.folder:
                try:
                    directory=self._preserve_failure()
                    print('Native Core exited unexpectedly; local diagnostics: '+str(directory),file=sys.stderr)
                except OSError as failure:
                    print('Could not retain native Core diagnostics: '+str(failure),file=sys.stderr)
            if self.folder:self.folder.cleanup()

"""Runner-only semantic baseline using the production CLI and one real native Engine.

--prepare-only writes deterministic source data without executing native code.
Other modes require explicit --ci and a GitHub-hosted-runner environment.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import queue
import shutil
import statistics
import sys
import subprocess
import threading
import time

PROTOCOL = 'cycode-core/1'


def position(text, needle, offset=0):
    byte = text.index(needle) + offset
    before = text[:byte]
    return {'line': before.count('\n'), 'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}


def summarize(samples):
    ordered = sorted(samples)
    return {'samplesMilliseconds': samples, 'medianMilliseconds': statistics.median(samples),
            'p95Milliseconds': ordered[max(0, math.ceil(len(ordered)*.95)-1)]}


def corpus(directory, units):
    directory.mkdir(parents=True, exist_ok=False)
    sources = {'widget.hpp': '#pragma once\n#ifndef CYCODE_BENCH_FACTOR\n#error missing compilation context\n#endif\nstruct Widget { int value; int reset() { return value * CYCODE_BENCH_FACTOR; } };\n',
               'main.cpp': '#include "widget.hpp"\n// Unicode: été 🛠\nint use_widget() { Widget w{1}; return w.value; }\n'}
    for index in range(units):
        sources[f'unit_{index:04d}.cpp'] = f'#include "widget.hpp"\nint unit_{index:04d}(Widget w) {{ return w.value + {index}; }}\n'
    commands = []
    for name, text in sources.items():
        (directory/name).write_text(text, encoding='utf-8', newline='\n')
        if name.endswith('.cpp'):
            commands.append({'directory': str(directory), 'file': str(directory/name),
                'arguments': ['clang++', '-std=c++20', '-DCYCODE_BENCH_FACTOR=2', '-I'+str(directory), '-c', str(directory/name)]})
    (directory/'compile_commands.json').write_text(json.dumps(commands, indent=2)+'\n', encoding='utf-8')
    digest = hashlib.sha256()
    for name, text in sorted(sources.items()):
        digest.update(name.encode()); digest.update(b'\0'); digest.update(text.encode()); digest.update(b'\0')
    return {'kind': 'synthetic-cpp', 'translationUnits': len(commands), 'sourceFiles': len(sources),
            'sourceBytes': sum(len(t.encode()) for t in sources.values()), 'sourceSha256': digest.hexdigest(),
            'compileArguments': ['clang++','-std=c++20','-DCYCODE_BENCH_FACTOR=2','-I<corpus>','-c','<source>']}


def linux_memory(root_pid):
    """RSS sum includes shared pages more than once; record this explicitly."""
    if platform.system() != 'Linux':
        return {'available': False, 'reason': 'Initial memory sampler supports Linux /proc only'}
    processes = {}
    for item in Path('/proc').iterdir():
        if not item.name.isdigit(): continue
        try:
            fields = dict(line.split(':',1) for line in (item/'status').read_text().splitlines() if ':' in line)
            processes[int(item.name)] = (int(fields['PPid'].strip()), int(fields.get('VmRSS','0 kB').split()[0])*1024,
                                         fields['Name'].strip())
        except (OSError, ValueError, KeyError): continue
    selected = {root_pid}
    while True:
        children = {pid for pid,(parent,_,_) in processes.items() if parent in selected}
        if children <= selected: break
        selected.update(children)
    rows = [{'pid':pid,'name':processes[pid][2],'rssBytes':processes[pid][1]} for pid in sorted(selected) if pid in processes]
    return {'available': bool(rows), 'processes': rows, 'sumRssBytes': sum(row['rssBytes'] for row in rows),
            'scope': 'CLI plus live descendants; instantaneous RSS, shared pages may be counted repeatedly; not peak/private memory'}


class Host:
    def __init__(self, cli, workspace, log):
        self.stderr = log.open('wb')
        self.started = time.perf_counter()
        self.process = subprocess.Popen([str(cli),'--disable-plugins',str(workspace),'--stdio'],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.stderr)
        self.lines = queue.Queue(maxsize=4)
        self.reader = threading.Thread(target=self._read, daemon=True); self.reader.start()
        self.sequence = 0
        try:
            ready = self.receive(45)
            if ready.get('event')!='ready' or ready.get('protocol')!=PROTOCOL:
                raise RuntimeError('Unexpected native handshake')
            self.startup_ms = (time.perf_counter()-self.started)*1000
            self.context = ready['status']['engine']['context']
        except BaseException:
            # Even a malformed/missing handshake must release our own host.
            # EOF first allows the same Engine shutdown as a normal session.
            self.process.stdin.close()
            try:
                self.process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.wait(timeout=10)
            self.reader.join(timeout=5)
            self.process.stdout.close(); self.stderr.close()
            raise

    def _read(self):
        try:
            while True:
                line = self.process.stdout.readline(20*1024*1024+1)
                if not line: break
                if len(line)>20*1024*1024 or not line.endswith(b'\n'):
                    self.lines.put(RuntimeError('Native response exceeds its line limit')); return
                self.lines.put(line)
        finally:
            self.lines.put(None)

    def receive(self, timeout=90):
        try: line = self.lines.get(timeout=timeout)
        except queue.Empty as e: raise RuntimeError('Native response timed out') from e
        if line is None: raise RuntimeError('Native process exited without a response')
        if isinstance(line,Exception): raise line
        return json.loads(line)

    def call(self, method, params=None, expected_error=None):
        self.sequence += 1
        request = {'protocol':PROTOCOL,'id':self.sequence,'method':method,'params':params or {}}
        encoded = (json.dumps(request,ensure_ascii=False)+'\n').encode()
        if len(encoded)>8*1024*1024: raise RuntimeError('Benchmark request exceeds native bound')
        self.process.stdin.write(encoded); self.process.stdin.flush()
        response = self.receive()
        if response.get('protocol')!=PROTOCOL or response.get('id')!=self.sequence:
            raise RuntimeError('Native response identity mismatch')
        if expected_error:
            if response.get('error',{}).get('code')!=expected_error: raise RuntimeError('Unexpected native error response')
            return response['error']
        if 'error' in response: raise RuntimeError(str(response['error']))
        return response['result']

    def close(self):
        try:
            if self.process.poll() is None:
                state = self.call('service.shutdown')
                if state['service']['state']!='draining': raise RuntimeError('Native host did not drain')
                self.process.stdin.close()
                self.process.wait(timeout=30)
            if self.process.returncode: raise RuntimeError('Native host failed shutdown')
        finally:
            if self.process.poll() is None:
                self.process.kill(); self.process.wait(timeout=10)
            self.process.stdout.close(); self.stderr.close(); self.reader.join(timeout=5)


def cpp_status(host):
    status = host.call('language.status')
    return status.get('servers',{}).get('cpp',status)


def wait_diagnostics(host, revision, timeout=45):
    deadline = time.monotonic()+timeout
    while time.monotonic()<deadline:
        status = cpp_status(host)
        publication = status.get('diagnosticRevisions',{}).get('main.cpp',{})
        if publication.get('revision')==revision:
            uri = publication['uri']
            if any(item.get('severity')==1 for item in status['diagnostics'].get(uri,[])):
                raise RuntimeError('Corpus has compiler errors; benchmark would not be meaningful')
            return status
        if status.get('error'): raise RuntimeError(status['error'])
        time.sleep(.02)
    raise RuntimeError('No version-bound diagnostics for the current native document')


def run_host(cli, clangd, workspace, report_directory, iteration, samples):
    host = Host(cli,workspace,report_directory/f'native-{iteration}.log')
    try:
        host.call('service.status')
        host.call('workspace.describe', expected_error=None)
        # A bad request must not destroy the project Engine.
        host.call('documents.read',{'path':'missing.cpp'}, expected_error='OperationFailed')
        if host.call('service.status')['engine']['context']!=host.context:
            raise RuntimeError('A request failure replaced the native context')
        analysis_begin = time.perf_counter()
        host.call('language.configure',{'language':'cpp','executable':str(clangd),'enabled':True})
        doc = host.call('documents.read',{'path':'main.cpp'})
        header = host.call('documents.read',{'path':'widget.hpp'})
        status = wait_diagnostics(host,doc['revision'])
        analysis_ms = (time.perf_counter()-analysis_begin)*1000
        epoch = status['epoch']
        timings = {}
        location = position(doc['text'],'w.value',2)
        base = {'path':'main.cpp','expectedRevision':doc['revision'],'position':location}
        operations = [('definition','language.definition',base),
                      ('references','language.references',{**base,'includeDeclaration':True}),
                      ('completion','language.complete',{**base,'position':position(doc['text'],'w.value',2)}),
                      ('renamePreview','language.rename',{**base,'newName':'amount'})]
        for name, method, params in operations:
            observed=[]
            for _ in range(samples):
                before=time.perf_counter(); result=host.call(method,params); observed.append((time.perf_counter()-before)*1000)
                if name in ('definition','references') and not result.get('locations'):
                    raise RuntimeError(name+' returned no semantic locations')
                if name=='completion':
                    payload=result.get('result',{})
                    items=payload.get('items',[]) if isinstance(payload,dict) else payload
                    if not any(item.get('label')=='value' for item in items): raise RuntimeError('Completion lost the actual member')
                if name=='renamePreview':
                    if result.get('state')!='preview' or not result.get('editCount',0): raise RuntimeError('Rename returned no guarded preview')
                    # No apply/save here: preview timings include native preparation only.
            timings[name]=summarize(observed)
        if cpp_status(host)['epoch']!=epoch: raise RuntimeError('Warm requests restarted the language backend')
        # Unsaved declarations and references must be visible together, disk bytes unchanged.
        disk_before={name:hashlib.sha256((workspace/name).read_bytes()).hexdigest() for name in ['main.cpp','widget.hpp']}
        overlay=[]
        for step in range(min(samples,5)):
            text=doc['text']+f'// rapid unsaved edit {step}\n'
            before=time.perf_counter()
            doc=host.call('documents.replace',{'path':'main.cpp','expectedRevision':doc['revision'],'text':text})
            wait_diagnostics(host,doc['revision'])
            query=host.call('language.definition',{'path':'main.cpp','expectedRevision':doc['revision'],'position':location})
            if not query.get('locations'): raise RuntimeError('Dirty document lost semantic navigation')
            overlay.append((time.perf_counter()-before)*1000)
        if any(hashlib.sha256((workspace/name).read_bytes()).hexdigest()!=digest for name,digest in disk_before.items()):
            raise RuntimeError('Benchmark unexpectedly saved source files')
        if host.call('service.status')['engine']['context']!=host.context: raise RuntimeError('Warm operations replaced the Engine')
        return {'processColdStartupMilliseconds':host.startup_ms,
                'firstVersionBoundDiagnosticsMilliseconds':analysis_ms,
                'warmOperations':timings,'unsavedEditDiagnosticsAndDefinition':summarize(overlay),
                'backendEpoch':epoch,'memory':linux_memory(host.process.pid),
                'checks':{'persistentContext':True,'persistentBackendEpoch':True,'diskSourcesUnchanged':True}}
    finally:
        host.close()


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ci',action='store_true')
    parser.add_argument('--prepare-only',action='store_true')
    parser.add_argument('--cli',type=Path)
    parser.add_argument('--clangd',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--units',type=int,default=32)
    parser.add_argument('--samples',type=int,default=7)
    parser.add_argument('--iterations',type=int,default=2)
    args=parser.parse_args(argv)
    if not 1<=args.units<=2000 or not 1<=args.samples<=30 or not 1<=args.iterations<=10:
        parser.error('Expected units=1..2000, samples=1..30, iterations=1..10')
    if not args.prepare_only and (not args.ci or os.environ.get('GITHUB_ACTIONS')!='true' or os.environ.get('CI')!='true' or not os.environ.get('RUNNER_OS')):
        parser.error('Native measurements require explicit --ci on a GitHub runner; local native execution remains suspended')
    if not args.prepare_only and (not args.cli or not args.clangd): parser.error('--cli and --clangd are required')
    output=args.output.resolve()
    # Never overwrite a user's or a failed benchmark's existing data.
    output.mkdir(parents=True,exist_ok=False)
    description=corpus(output/'corpus',args.units)
    report={'format':'cycode-semantic-baseline/1','corpus':description,
            'scope':'Synthetic C++ workload, production CLI JSON/stdio overhead included; process-cold is not OS-cache cold; no GUI, no file-search proxy',
            'platform':sys.platform if args.prepare_only else platform.platform(),'python':sys.version.split()[0],
            'limits':['No large real-world CMake/MSBuild/Unreal corpus yet','No whole-project index completion signal',
                      'No concurrent/cancellation/load-invalidation benchmark yet','No peak/private memory measurement'],
            'runs':[]}
    if args.prepare_only:
        report['execution']='not-run: source-data-only'
    else:
        cli,clangd=args.cli.resolve(),args.clangd.resolve()
        report['binarySha256']=hashlib.sha256(cli.read_bytes()).hexdigest()
        report['clangdVersion']=subprocess.check_output([str(clangd),'--version'],text=True,timeout=10)
        report['coreVersion']=subprocess.check_output([str(cli),'--version'],text=True,timeout=10).strip()
        for iteration in range(args.iterations):
            report['runs'].append(run_host(cli,clangd,output/'corpus',output,iteration,args.samples))
            (output/'baseline.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        report['execution']='completed'
    (output/'baseline.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Semantic baseline: '+str(output/'baseline.json'))
    return 0

if __name__=='__main__':
    raise SystemExit(main())

"""Explicit local inspection benchmark; measurements are not IDE feature parity."""
from pathlib import Path
import argparse,hashlib,json,math,os,statistics,subprocess,sys,threading,time

class MemorySample:
    """Observed process-tree RSS sum; shared pages can be counted more than once."""
    def __init__(self,pid):
        self.pid=pid;self.peak=0;self.samples=0;self.errors=0;self.stop=threading.Event();self.thread=None
    def __enter__(self):
        try:
            import psutil
            root=psutil.Process(self.pid);identity=root.create_time()
        except ImportError:return self
        except psutil.Error:return self
        def sample():
            while not self.stop.is_set():
                try:
                    if root.create_time()!=identity:break
                    processes=[root,*root.children(recursive=True)];seen=set();total=0
                    for item in processes:
                        if item.pid not in seen:
                            seen.add(item.pid)
                            try:total+=item.memory_info().rss
                            except psutil.Error:self.errors+=1
                    self.peak=max(self.peak,total);self.samples+=1
                except psutil.Error:break
                self.stop.wait(.05)
        self.thread=threading.Thread(target=sample,daemon=True);self.thread.start();return self
    def __exit__(self,*_):
        self.stop.set()
        if self.thread:self.thread.join(2)
    def report(self):return {'available':bool(self.samples),'peakObservedWorkingSetSumBytes':self.peak if self.samples else None,
        'samples':self.samples,'unavailableProcessObservations':self.errors,'scope':'Owned process tree sampled every 50 ms; RSS sum may double-count shared pages, not unique physical memory'}

def summarize(rows):
    values=sorted(r['seconds'] for r in rows)
    return {'samples':len(rows),'medianSeconds':statistics.median(values),'p95Seconds':values[math.ceil(.95*len(values))-1]}

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-native',action='store_true');parser.add_argument('--core',type=Path,required=True)
    parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--workspace',type=Path,required=True)
    parser.add_argument('--database',required=True);parser.add_argument('--source',action='append',required=True)
    parser.add_argument('--clang-tidy',type=Path,required=True);parser.add_argument('--jetbrains',type=Path)
    parser.add_argument('--configuration',default='Debug');parser.add_argument('--platform',default='x64');parser.add_argument('--solution');parser.add_argument('--samples',type=int,default=3);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(argv)
    if not args.run_native:parser.error('Native execution requires --run-native')
    if not 1<=args.samples<=20:parser.error('Samples must be between 1 and 20')
    root=args.workspace.resolve(strict=True);output=args.output.resolve();output.mkdir(exist_ok=False)
    assert not root.is_symlink() and len(set(args.source))==len(args.source)
    files={}
    for name in [*args.source,args.database]:
        path=(root/name).resolve(strict=True);assert path.is_relative_to(root) and path.is_file() and not path.is_symlink()
        files[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    sys.path.insert(0,str(args.core/'app'))
    from core_host import ManagedCore
    from tool import NativeClient
    report={'format':'cycode-inspection-comparison/1','sourcesSha256':files,'translationUnitsPerCoreSample':len(args.source),
        'nativeSha256':hashlib.sha256(args.executable.read_bytes()).hexdigest(),'core':[],'jetbrains':[],
        'limits':['Core: persistent native service plus one clang-tidy process per translation unit; native socket API, no SDK polling.',
            'JetBrains: new InspectCode process for each solution-wide run, reused caches after the first run.',
            'Recommended Clang checks and JetBrains default inspections differ; counts and timings do not prove equal analysis or overall IDE superiority.',
            'OS disk caches are not flushed; first run does not mean cold filesystem. Memory is an observed RSS sum.']}
    def save():(output/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    with ManagedCore(root,output/'core-host',args.executable,True) as host:
        client=NativeClient(host.descriptor)
        for index in range(args.samples):
            started=time.perf_counter();findings=[];statuses=[]
            with MemorySample(host.process.pid) as memory:
                for name in args.source:
                    doc=client.call('documents.read',{'path':name})
                    result=client.call('analysis.inspect',{'path':name,'expectedRevision':doc['revision'],'compilationDatabase':args.database,
                        'profile':'recommended','executable':str(args.clang_tidy),'timeoutSeconds':180})
                    assert result['status']=='complete' and not result['stale'],result
                    findings.extend(result['diagnostics']);statuses.append(result['status'])
            row={'seconds':time.perf_counter()-started,'findings':len(findings),'codes':sorted({d['code'] for d in findings}),
                'uninitializedDetected':any('uninitial' in (d['code']+' '+d['message']).lower() for d in findings),'statuses':statuses,'memory':memory.report()}
            report['core'].append(row);save();print(json.dumps({'engine':'CyCodeCore','sample':index,**row}),flush=True)
    if args.jetbrains:
        assert args.solution
        solution=(root/args.solution).resolve(strict=True);assert solution.is_relative_to(root)
        for index in range(args.samples):
            target=output/('jetbrains-'+str(index)+'.sarif');log=output/('jetbrains-'+str(index)+'.log')
            command=[str(args.jetbrains),'inspectcode',str(solution),'--output='+str(target),'--format=Sarif','--no-build','--no-updates',
                '--no-buildin-settings','--properties=Configuration='+args.configuration+';Platform='+args.platform,'--jobs=2',
                '--caches-home='+str(output/'jetbrains-cache'),'--verbosity=WARN','--severity=HINT']
            started=time.perf_counter()
            with log.open('wb') as stream:
                process=subprocess.Popen(command,cwd=root,start_new_session=os.name!='nt',stdout=stream,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                try:
                    with MemorySample(process.pid) as memory:code=process.wait(timeout=300)
                except BaseException:
                    if process.poll() is None:
                        if os.name=='nt':subprocess.run(['taskkill.exe','/PID',str(process.pid),'/T','/F'],capture_output=True)
                        else:
                            import signal
                            os.killpg(process.pid,signal.SIGTERM)
                    raise
            assert code==0 and target.is_file(),'InspectCode did not complete; inspect its log'
            data=json.loads(target.read_text('utf-8-sig'));findings=[item for run in data['runs'] for item in run.get('results',[])]
            row={'seconds':time.perf_counter()-started,'findings':len(findings),'codes':sorted({f.get('ruleId','') for f in findings}),
                'uninitializedDetected':any('uninitial' in (f.get('ruleId','')+' '+f.get('message',{}).get('text','')).lower() for f in findings),'memory':memory.report()}
            report['jetbrains'].append(row);save();print(json.dumps({'engine':'JetBrains InspectCode','sample':index,**row}),flush=True)
    for name,want in files.items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==want,'Source or context changed during benchmark'
    report['sourceAndDatabasePreserved']=True;report['summary']={'core':summarize(report['core'])}
    if report['jetbrains']:report['summary']['jetbrains']=summarize(report['jetbrains'])
    report['state']='complete';save()
if __name__=='__main__':main()

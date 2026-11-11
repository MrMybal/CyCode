"""Budgeted actual UE compiler traversal with native restart-persistent progress."""
from pathlib import Path
import argparse,asyncio,hashlib,json,math,os,statistics,sys,time
import psutil
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client

parser=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','clangd','workspace','database','output'):parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--run-native',action='store_true');parser.add_argument('--checkpoint')
parser.add_argument('--max-units',type=int,default=0,help='0 traverses the remaining inventory until the time budget')
parser.add_argument('--max-seconds',type=int,default=600,help='0 continues until inventory completion or an explicit stop file; positive values stop with resumable progress');parser.add_argument('--batch-size',type=int,default=8)
parser.add_argument('--stop-file',type=Path,help='Creating this file requests cooperative cancellation; it is never removed')
A=parser.parse_args()
if not A.run_native:parser.error('Explicit --run-native is required')
if A.max_units<0 or not 0<=A.max_seconds<=604800 or not 1<=A.batch_size<=256:parser.error('Invalid campaign bounds')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False)
root=A.workspace.resolve(strict=True);db=A.database.resolve(strict=True);assert db.is_relative_to(root)
def digest_file(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(65536),b''):value.update(block)
    return value.hexdigest()
database_hash=digest_file(db)
sys.path.insert(0,str(A.core/'app/scripts'))
from benchmark_inspections import MemorySample
report={'format':'cycode-unreal-campaign/1','state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),
    'compilationDatabaseSha256':database_hash,'batches':[],'documents':[],'documentsScope':'latest-batch',
    'observationsFile':'documents.jsonl','observationsCount':0,'timeBudgetSeconds':A.max_seconds,'unlimitedDuration':A.max_seconds==0,
    'limits':['Current-process compiler observations and historical traversal are separate.',
              'Budget exhaustion is pending progress, not a completed whole-project analysis.',
              'Observed process-tree RSS can miss peaks and count shared pages repeatedly.']}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(result):
    if result.isError:raise RuntimeError(result.content[0].text)
    return json.loads(result.content[0].text)
async def submit(s,name,args):return decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId']
stops_enabled=False
async def wait(s,job,deadline=None):
    cancelled=False
    while True:
        value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
        if value['state']=='Completed':return value['outputs']
        if value['state']=='Cancelled' and cancelled:return None
        if value['state'] in ('Failed','Cancelled'):raise RuntimeError(json.dumps(value))
        if not cancelled and ((deadline is not None and time.monotonic()>=deadline) or (stops_enabled and A.stop_file and A.stop_file.exists())):
            await s.call_tool('cy_cancel_job',{'jobId':job});cancelled=True
        await asyncio.sleep(.1)
async def call(s,name,args,deadline=None):return await wait(s,await submit(s,name,args),deadline)
async def main():
    global stops_enabled
    started=time.perf_counter();data=OUT/'jobs'
    parameters=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(root),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(data),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(parameters,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize()
        await call(s,'language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','--background-index-priority=low','-j=2','--use-dirty-headers','--log=error','--header-insertion=never','--compile-commands-dir='+str(db.parent)]})
        children=psutil.Process().children(recursive=True)
        pid=next(p.pid for p in children if p.name().lower()==A.executable.name.lower())
        with MemorySample(pid) as memory:
            initial={'checkpoint':A.checkpoint,'batchSize':0} if A.checkpoint else {'scope':'compilation-database','persist':True,'batchSize':0}
            result=await call(s,'analysis_prepare',initial)
            report.update(checkpoint=result['checkpoint'],total=result['total'],startObserved=result['completed'],
                historicalObserved=result.get('historicalObserved',result.get('previousProcessObserved',0)),initialCoverage=result['coverage'])
            stops_enabled=True
            cursor=result['completed'];initial_cursor=cursor;deadline=time.monotonic()+A.max_seconds if A.max_seconds else None
            save()
            # Hash only the selected upcoming sources, using the same genuine UBT
            # inventory. These reads do not replace compiler semantic observations.
            commands=json.loads(db.read_text('utf-8'));paths=set()
            for command in commands:
                path=Path(command['file']);path=path if path.is_absolute() else Path(command['directory'])/path
                path=path.resolve()
                if path.is_relative_to(root):paths.add(path.relative_to(root).as_posix())
            paths=sorted(paths);assert len(paths)==result['total']
            checked_sources=0;unreadable_sources=0;timings=[]
            del commands
            while result['continuation'] and (deadline is None or time.monotonic()<deadline) and not (A.stop_file and A.stop_file.exists()):
                remaining=A.max_units-(cursor-initial_cursor) if A.max_units else A.batch_size
                if remaining<=0:break
                size=min(A.batch_size,remaining)
                baseline={}
                for name in paths[cursor:cursor+size]:
                    try:baseline[name]=digest_file(root/name)
                    except OSError:unreadable_sources+=1
                before=time.perf_counter()
                value=await call(s,'analysis_prepare',{'continuation':result['continuation'],'batchSize':size},deadline)
                if value is None:
                    value=await call(s,'analysis_status',{})
                    value['continuation']=value['session'];value['wholeProjectIndexCompleteness']='unknown'
                count=value['completed']-cursor;rows=[];offset=cursor
                while offset<value['completed']:
                    page=await call(s,'analysis_results',{'session':value['session'],'offset':offset,'limit':256})
                    rows.extend(page['documents']);offset=page['nextOffset']
                assert len(rows)==count
                with (OUT/'documents.jsonl').open('a',encoding='utf-8',newline='\n') as stream:
                    for row in rows:stream.write(json.dumps(row,ensure_ascii=False)+'\n')
                    stream.flush();os.fsync(stream.fileno())
                report['documents']=rows
                report['observationsCount']+=len(rows)
                timings.extend(row['elapsedMilliseconds'] for row in rows)
                for row in rows:
                    if row['path'] in baseline:
                        assert digest_file(root/row['path'])==baseline[row['path']], 'Source changed during its compiler batch'
                        checked_sources+=1
                report['batches'].append({'observed':value['completed'],'added':count,'seconds':time.perf_counter()-before,'coverage':value['coverage']})
                cursor=value['completed'];result=value;save()
                print(json.dumps({'observed':cursor,'total':result['total'],'coverage':result['coverage'],'memory':memory.report()}),flush=True)
                if result['state']=='cancelled':break
            report.update(finalObserved=cursor,addedThisRun=cursor-initial_cursor,finalCoverage=result['coverage'],
                traversalComplete=cursor==result['total'],state='completed' if cursor==result['total'] else 'pending',
                wholeProjectIndexCompleteness='unknown',memory=memory.report())
            timings.sort()
            if timings:report['unitTimingMilliseconds']={'samples':len(timings),'median':statistics.median(timings),'p95':timings[math.ceil(.95*len(timings))-1],'maximum':max(timings)}
            assert digest_file(db)==database_hash
            report['checkedSourceHashes']=checked_sources;report['unreadableBaselineSources']=unreadable_sources
            report['sourcesPreservedAtEachBatch']=True;report['compilationDatabasePreserved']=True
            report['selectedSourcesAndDatabasePreserved']=True
            observations=OUT/'documents.jsonl'
            if observations.exists():report['observationsSha256']=digest_file(observations)
            report['stopReason']='complete' if cursor==result['total'] else 'stop-file' if A.stop_file and A.stop_file.exists() else 'time-budget' if deadline is not None and time.monotonic()>=deadline else 'unit-budget'
            report['secondsBeforeShutdown']=time.perf_counter()-started;save()
    assert not list(data.glob('native-core-*'))
    report['hostDrained']=True;report['totalSeconds']=time.perf_counter()-started;save()
    print(json.dumps({'state':report['state'],'added':report['addedThisRun'],'checkpoint':report['checkpoint'],'hostDrained':True}),flush=True)
try:asyncio.run(main())
except BaseException as error:
    report.update(state='failed',error=str(error));save();raise

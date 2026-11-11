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
parser.add_argument('--max-seconds',type=int,default=600);parser.add_argument('--batch-size',type=int,default=8)
A=parser.parse_args()
if not A.run_native:parser.error('Explicit --run-native is required')
if A.max_units<0 or not 1<=A.max_seconds<=900 or not 1<=A.batch_size<=256:parser.error('Invalid campaign bounds')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False)
root=A.workspace.resolve(strict=True);db=A.database.resolve(strict=True);assert db.is_relative_to(root)
database_hash=hashlib.sha256(db.read_bytes()).hexdigest()
sys.path.insert(0,str(A.core/'app/scripts'))
from benchmark_inspections import MemorySample
report={'format':'cycode-unreal-campaign/1','state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),
    'compilationDatabaseSha256':database_hash,'batches':[],'documents':[],
    'limits':['Current-process compiler observations and historical traversal are separate.',
              'Budget exhaustion is pending progress, not a completed whole-project analysis.',
              'Observed process-tree RSS can miss peaks and count shared pages repeatedly.']}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def decode(result):
    if result.isError:raise RuntimeError(result.content[0].text)
    return json.loads(result.content[0].text)
async def submit(s,name,args):return decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId']
async def wait(s,job,deadline=None):
    cancelled=False
    while True:
        value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
        if value['state']=='Completed':return value['outputs']
        if value['state']=='Cancelled' and cancelled:return None
        if value['state'] in ('Failed','Cancelled'):raise RuntimeError(json.dumps(value))
        if deadline and not cancelled and time.monotonic()>=deadline:
            await s.call_tool('cy_cancel_job',{'jobId':job});cancelled=True
        await asyncio.sleep(.1)
async def call(s,name,args,deadline=None):return await wait(s,await submit(s,name,args),deadline)
async def main():
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
            cursor=result['completed'];initial_cursor=cursor;deadline=time.monotonic()+A.max_seconds
            # Hash only the selected upcoming sources, using the same genuine UBT
            # inventory. These reads do not replace compiler semantic observations.
            commands=json.loads(db.read_text('utf-8'));paths=set()
            for command in commands:
                path=Path(command['file']);path=path if path.is_absolute() else Path(command['directory'])/path
                path=path.resolve()
                if path.is_relative_to(root):paths.add(path.relative_to(root).as_posix())
            paths=sorted(paths);assert len(paths)==result['total']
            selected=paths[cursor:cursor+(A.max_units or len(paths))]
            baseline={}
            for name in selected:
                try:baseline[name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
                except OSError:pass
            del commands
            while result['continuation'] and time.monotonic()<deadline:
                remaining=A.max_units-(cursor-initial_cursor) if A.max_units else A.batch_size
                if remaining<=0:break
                size=min(A.batch_size,remaining);before=time.perf_counter()
                value=await call(s,'analysis_prepare',{'continuation':result['continuation'],'batchSize':size},deadline)
                if value is None:
                    value=await call(s,'analysis_status',{})
                    value['continuation']=value['session'];value['wholeProjectIndexCompleteness']='unknown'
                count=value['completed']-cursor;rows=[];offset=cursor
                while offset<value['completed']:
                    page=await call(s,'analysis_results',{'session':value['session'],'offset':offset,'limit':256})
                    rows.extend(page['documents']);offset=page['nextOffset']
                assert len(rows)==count
                report['documents'].extend(rows)
                report['batches'].append({'observed':value['completed'],'added':count,'seconds':time.perf_counter()-before,'coverage':value['coverage']})
                cursor=value['completed'];result=value;save()
                print(json.dumps({'observed':cursor,'total':result['total'],'coverage':result['coverage'],'memory':memory.report()}),flush=True)
                if result['state']=='cancelled':break
            report.update(finalObserved=cursor,addedThisRun=cursor-initial_cursor,finalCoverage=result['coverage'],
                traversalComplete=cursor==result['total'],state='completed' if cursor==result['total'] else 'pending',
                wholeProjectIndexCompleteness='unknown',memory=memory.report())
            timings=sorted(row['elapsedMilliseconds'] for row in report['documents'])
            if timings:report['unitTimingMilliseconds']={'samples':len(timings),'median':statistics.median(timings),'p95':timings[math.ceil(.95*len(timings))-1],'maximum':max(timings)}
            assert all(hashlib.sha256((root/name).read_bytes()).hexdigest()==digest for name,digest in baseline.items())
            assert hashlib.sha256(db.read_bytes()).hexdigest()==database_hash
            report['checkedSourceHashes']=len(baseline);report['selectedSourcesAndDatabasePreserved']=True
            report['secondsBeforeShutdown']=time.perf_counter()-started;save()
    assert not list(data.glob('native-core-*'))
    report['hostDrained']=True;report['totalSeconds']=time.perf_counter()-started;save()
    print(json.dumps({'state':report['state'],'added':report['addedThisRun'],'checkpoint':report['checkpoint'],'hostDrained':True}),flush=True)
try:asyncio.run(main())
except BaseException as error:
    report.update(state='failed',error=str(error));save();raise

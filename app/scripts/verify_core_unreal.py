"""Real UE regression: existing UBT database, resumable scope, no source saves."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser();P.add_argument('--core',type=Path,required=True);P.add_argument('--executable',type=Path,required=True);P.add_argument('--workspace',type=Path,required=True);P.add_argument('--database',type=Path,required=True);P.add_argument('--output',type=Path,required=True)
P.add_argument('--run-native',action='store_true')
P.add_argument('--clangd',type=Path,required=True)
A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; no Core is launched by default')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False)
paths=['Engine/Source/Runtime/Core/Public/Misc/DateTime.h','Engine/Source/Runtime/Core/Private/Misc/DateTime.cpp','Engine/Source/Runtime/Online/HTTP/Private/HttpRetrySystem.cpp']
baseline={p:hashlib.sha256((A.workspace/p).read_bytes()).hexdigest() for p in paths}
dbhash=hashlib.sha256(A.database.read_bytes()).hexdigest();commands=len(json.loads(A.database.read_text('utf-8')))
report={'state':'running','sourceSha256':baseline,'compilationDatabaseSha256':dbhash,'ubtCommands':commands,'checks':{},'limits':['Scoped source regression, not exhaustive UE semantics, reflection/Blueprint analysis or Rider integration parity.']}
def mark(name,value=True):report['checks'][name]=value;(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({'stage':name,'value':value}),flush=True)
def decode(result):
    if result.isError:raise RuntimeError(result.content[0].text)
    return json.loads(result.content[0].text)
async def main():
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(A.workspace),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as session:
        await session.initialize()
        async def call(name,args):
            job=decode(await session.call_tool('op_'+name,{'parameters':args}))['jobId'];until=time.monotonic()+600
            while time.monotonic()<until:
                value=decode(await session.call_tool('cy_get_job',{'jobId':job}))
                if value['state']=='Completed':return value['outputs']
                if value['state'] in ('Failed','Cancelled'):raise RuntimeError(name+': '+json.dumps(value))
                await asyncio.sleep(.05)
            decode(await session.call_tool('cy_cancel_job',{'jobId':job}));raise TimeoutError(name)
        await call('language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','--background-index-priority=low','-j=2','--use-dirty-headers','--log=error','--header-insertion=never','--compile-commands-dir='+str(A.database.parent)]})
        inventory=await call('analysis_prepare',{'scope':'compilation-database','batchSize':0})
        assert inventory['total']>30000 and inventory['continuation'];mark('wholeDatabaseInventory',inventory['total'])
        resumed=await call('analysis_prepare',{'continuation':inventory['continuation'],'batchSize':2})
        assert resumed['total']==inventory['total'] and resumed['coverage']['observed']==2 and resumed['coverage']['remaining']==inventory['total']-2
        mark('rollingPreparationContinued',resumed['coverage'])
        prepared=await call('analysis_prepare',{'paths':paths});assert prepared['coverage']['diagnosticErrors']==0,prepared
        mark('dateTimeAndHttpCompilerScope',prepared['coverage'])
        doc=await call('documents_read',{'path':paths[0]});text=doc['text'];offset=text.index('ParseHttpDate(const FString');prefix=text[:offset]
        position={'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
        plan=await call('language_rename',{'path':paths[0],'expectedRevision':doc['revision'],'position':position,'newName':'ParseHttpDate_CyCodeValidation'})
        assert len(plan['files'])==3,plan
        await call('changes_apply',{'id':plan['id']});await call('changes_undo',{'id':plan['id']});mark('realThreeFileRenameApplyUndo')
        assert all(hashlib.sha256((A.workspace/p).read_bytes()).hexdigest()==digest for p,digest in baseline.items())
        assert hashlib.sha256(A.database.read_bytes()).hexdigest()==dbhash;mark('unrealSourcesAndDatabasePreserved')
    assert not list((OUT/'jobs').glob('native-core-*'));mark('hostDrained');report['state']='passed';(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise

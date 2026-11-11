"""Real durable paged recovery, including one owned production-host crash."""
from pathlib import Path
from contextlib import asynccontextmanager
import argparse,asyncio,hashlib,json,os,sys,time
import psutil
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client

P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; this campaign terminates one owned Core after a durable checkpoint')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
source=WORK/'Recovery.log';original=b'\xef\xbb\xbf'+b'line\r\n'*1000000+'last été 🛠\r\n'.encode();source.write_bytes(original)
inserted='recovered été 🛠\r\n'.encode();expected=original+inserted
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':{},'sourceBytes':len(original)}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def mark(name,value=True):report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def decode(r):
    if r.isError:raise RuntimeError(r.content[0].text)
    return json.loads(r.content[0].text)
@asynccontextmanager
async def host(label):
    data=OUT/label
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(data),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/(label+'-stderr.log')).open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as s:
        await s.initialize()
        async def call(name,args,fail=False):
            job=decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId'];until=time.monotonic()+240
            while time.monotonic()<until:
                value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):
                    if fail:assert value['state']=='Failed',value;return value
                    assert value['state']=='Completed',(name,value);return value['outputs']
                await asyncio.sleep(.02)
            raise TimeoutError(name)
        yield call
    assert not list(data.glob('native-core-*/native.json')), 'Native authentication descriptor was not drained'
async def main():
    async with host('first-jobs') as call:
        doc=await call('documents_read_page',{'path':source.name})
        edit=await call('documents_prepare_byte_edit',{'path':source.name,'expectedRevision':doc['revision'],'offset':len(original),'length':0,'text':inserted.decode()})
        await call('changes_apply',{'id':edit['id']});doc=await call('documents_read_page',{'path':source.name})
        checkpoint=await call('documents_checkpoint_paged',{'path':source.name,'expectedRevision':doc['revision']})
        assert source.read_bytes()==original
        preview=await call('documents_preview_paged_checkpoint',{'checkpoint':checkpoint['checkpoint'],'offset':len(original),'length':70})
        assert preview['text'].encode()==inserted and preview['diskMatchesBaseline']
        candidates=[p for p in psutil.Process().children(recursive=True) if p.name().lower()==A.executable.name.lower()]
        assert len(candidates)==1,'Ambiguous production host ownership'
        candidates[0].terminate();candidates[0].wait(15)
        mark('ownedHostCrashAfterDurableCheckpoint',{'checkpointBytes':checkpoint['bytes'],'sourcePreserved':True,'scope':'Terminate exactly the one descendant production Core after publication, not an OS/power-loss test'})
    async with host('second-jobs') as call:
        found=await call('documents_paged_checkpoints',{})
        assert not found['payloadDigestsVerified'] and any(r['checkpoint']==checkpoint['checkpoint'] for r in found['checkpoints'])
        doc=await call('documents_read_page',{'path':source.name})
        await call('documents_restore_paged_checkpoint',{'path':source.name,'expectedRevision':doc['revision']+1,'checkpoint':checkpoint['checkpoint']},True)
        plan=await call('documents_restore_paged_checkpoint',{'path':source.name,'expectedRevision':doc['revision'],'checkpoint':checkpoint['checkpoint']})
        view=await call('changes_preview',{'id':plan['id'],'path':source.name,'beforeOffset':len(original),'afterOffset':len(original),'length':70})
        assert view['before']=='' and view['after'].encode()==inserted
        await call('changes_apply',{'id':plan['id']});restored=await call('documents_read_page',{'path':source.name,'offset':len(original),'expectedRevision':doc['revision']+1})
        assert restored['dirty'] and restored['text'].encode()==inserted and source.read_bytes()==original
        await call('changes_undo',{'id':plan['id']});clean=await call('documents_read_page',{'path':source.name});assert not clean['dirty']
        source.write_bytes(original+b'external')
        changed=await call('documents_preview_paged_checkpoint',{'checkpoint':checkpoint['checkpoint'],'offset':len(original),'length':70});assert not changed['diskMatchesBaseline']
        await call('documents_restore_paged_checkpoint',{'path':source.name,'expectedRevision':clean['revision'],'checkpoint':checkpoint['checkpoint']},True)
        assert source.read_bytes()==original+b'external';source.write_bytes(original)
        plan=await call('documents_restore_paged_checkpoint',{'path':source.name,'expectedRevision':clean['revision'],'checkpoint':checkpoint['checkpoint']})
        await call('changes_apply',{'id':plan['id']});restored=await call('documents_read_page',{'path':source.name})
        await call('documents_save_paged_as',{'path':source.name,'destination':'Recovered.log','expectedRevision':restored['revision']})
        assert (WORK/'Recovered.log').read_bytes()==expected and source.read_bytes()==original
        folder=WORK/'.cycode/working-data'/checkpoint['checkpoint'];content=folder/'blob-1.bin'
        with content.open('r+b') as f:f.seek(4);value=f.read(1);f.seek(4);f.write(b'X')
        await call('documents_preview_paged_checkpoint',{'checkpoint':checkpoint['checkpoint']},True)
        with content.open('r+b') as f:f.seek(4);f.write(value)
        manifest=folder/'paged-checkpoint.json';before=manifest.read_bytes();meta=json.loads(before)
        nested=0
        for _ in range(12):nested={'nested':nested}
        meta['untrusted']=nested;manifest.write_text(json.dumps(meta),encoding='utf-8')
        await call('documents_preview_paged_checkpoint',{'checkpoint':checkpoint['checkpoint']},True)
        assert (await call('documents_paged_checkpoints',{}))['invalid']>=1
        manifest.write_bytes(before)
        mark('crossProcessRecoveryApplyUndoAndSaveAs',{'sourcePreserved':True,'recoveredBytesVerified':True,'staleRevisionAndDiskConflictRefused':True,'tamperedPayloadAndDeepMetadataRefused':True,'checkpointRetained':manifest.is_file()})
    report['state']='passed';mark('hostsDrained')
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

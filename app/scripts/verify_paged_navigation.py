"""Real native paged line/Save-as acceptance through the official MCP runtime."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client

P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; no native process starts by default')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
rows=['a'*65535+'\r\n']+[f'{i:05d} été 🛠 '+('x'*235)+('\r\n' if i%3==0 else '\r' if i%3==1 else '\n') for i in range(1,24000)]
rows.append('last 🛠')
original=b'\xef\xbb\xbf'+''.join(rows).encode('utf-8');source=WORK/'Large.txt';source.write_bytes(original)
(WORK/'Keep.txt').write_text('existing target',encoding='utf-8')
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':{},'sourceBytes':len(original)}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def mark(name,value=True):report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def decode(r):
    if r.isError:raise RuntimeError(r.content[0].text)
    return json.loads(r.content[0].text)
async def main():
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
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
        doc=await call('documents_read_page',{'path':'Large.txt'})
        async def line(n,character=0,length=70,fail=False):
            return await call('documents_read_lines',{'path':doc['path'],'expectedRevision':doc['revision'],'line':n,'character':character,'length':length},fail)
        first=await line(0);assert first['offset']==3 and first['totalLines']==len(rows) and first['text']=='a'*70
        for n in (1,2,3,1023,1024,1025,2047,2048,23999,24000):
            row=await line(n);assert rows[n].encode().startswith(row['text'].encode()) and row['linePositionEncoding']=='utf16',row
            assert original[row['offset']:row['nextOffset']]==row['text'].encode()
        await line(24001,fail=True)
        # The final emoji occupies two UTF-16 units; its middle is not a source position.
        end=await line(24000,5);assert end['text']=='🛠'
        await line(24000,6,fail=True);await line(24000,8,fail=True)
        crlf=await line(0,65532,4);assert crlf['text']=='aaa' and crlf['nextLine']==0 and crlf['nextCharacter']==65535
        follow=await line(crlf['nextLine'],crlf['nextCharacter'],20);assert follow['text'].startswith('\r\n00001')
        # A sparse anchor exactly on a CRLF and a chunk edge must skip both bytes.
        mark('sparseLineIndexUnicodeAndMixedNewlines',{'lines':len(rows),'bomExcludedOnlyFromLineCoordinates':True,'byteChunkBoundaryCrlf':True,'surrogateSplitRefused':True})
        edit=await call('documents_prepare_byte_edit',{'path':'Large.txt','expectedRevision':doc['revision'],'offset':3,'length':0,'text':'new 🛠\n'})
        await call('changes_apply',{'id':edit['id']});doc=await call('documents_read_page',{'path':'Large.txt'})
        changed=await line(0);assert changed['text'].startswith('new 🛠\n') and changed['totalLines']==len(rows)+1
        assert (await line(1))['text']=='a'*70
        await call('documents_save_paged_as',{'path':'Large.txt','destination':'Keep.txt','expectedRevision':doc['revision']},True)
        assert (WORK/'Keep.txt').read_text('utf-8')=='existing target'
        await call('documents_save_paged_as',{'path':'Large.txt','destination':'missing/New.txt','expectedRevision':doc['revision']},True)
        source.write_bytes(original+b'external')
        await call('documents_save_paged_as',{'path':'Large.txt','destination':'Conflict.txt','expectedRevision':doc['revision']},True)
        assert not (WORK/'Conflict.txt').exists();source.write_bytes(original)
        saved=await call('documents_save_paged_as',{'path':'Large.txt','destination':'Copy.txt','expectedRevision':doc['revision']})
        assert saved['id']==doc['id'] and saved['revision']!=doc['revision'] and saved['previousPath']=='Large.txt' and not saved['dirty']
        assert source.read_bytes()==original
        expected=original[:3]+'new 🛠\n'.encode()+original[3:];assert (WORK/'Copy.txt').read_bytes()==expected
        await call('changes_undo',{'id':edit['id']},True)
        assert (WORK/'Copy.txt').read_bytes()==expected and source.read_bytes()==original
        doc=saved;assert (await line(0))['text'].startswith('new 🛠\n')
        await call('documents_read_lines',{'path':'Copy.txt','expectedRevision':saved['revision']-1,'line':0},True)
        await call('documents_close_paged',{'path':'Copy.txt','expectedRevision':saved['revision']})
        mark('exclusiveStreamedSaveAs',{'identityPreserved':True,'oldPlansStale':True,'existingAndMissingAndChangedSourcesRefused':True,'originalSourcePreserved':True,'newBytesVerified':True})
    assert not list((OUT/'jobs').glob('native-core-*'));report['state']='passed';mark('hostDrained')
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

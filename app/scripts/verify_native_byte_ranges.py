"""Opt-in production stdio checks for byte ranges; no generated native executable."""
from pathlib import Path
import argparse,hashlib,json,subprocess
P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required')
out=A.output.resolve();out.mkdir(exist_ok=False);work=out/'workspace';work.mkdir()
source=work/'Text.txt';original=b'alpha\nbeta\n';source.write_bytes(original)
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':[]};sequence=0
with (out/'stderr.log').open('w',encoding='utf-8') as log:
 host=subprocess.Popen([str(A.executable),'--disable-plugins',str(work),'--stdio'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,text=True,encoding='utf-8',creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
 try:
  ready=json.loads(host.stdout.readline());assert ready.get('event')=='ready' and ready.get('protocol')=='cycode-core/1',ready
  def call(method,params,fail=False):
   global sequence
   sequence+=1;host.stdin.write(json.dumps({'protocol':'cycode-core/1','id':sequence,'method':method,'params':params})+'\n');host.stdin.flush()
   raw=host.stdout.readline();assert raw,'Native stdio disconnected';reply=json.loads(raw);assert reply['id']==sequence,reply
   if fail:assert reply.get('error',{}).get('code')=='InvalidParams',reply;return
   assert 'result' in reply,reply;return reply['result']
  opened=call('documents.readPage',{'path':'Text.txt','length':4})
  for field,value in [('offset',10**80),('offset',2**64),('offset',-.5),('offset',-1),('length',10**80),('length',-1)]:
   call('documents.readPage',{'path':'Text.txt','expectedRevision':opened['revision'],field:value},True)
  page=call('documents.readPage',{'path':'Text.txt','expectedRevision':opened['revision'],'offset':2.0,'length':4.0});assert page['offset']==2 and page['text']=='pha\n'
  call('documents.prepareByteEdit',{'path':'Text.txt','expectedRevision':opened['revision'],'offset':10**80,'length':1,'text':'Z'},True)
  plan=call('documents.prepareByteEdit',{'path':'Text.txt','expectedRevision':opened['revision'],'offset':1.0,'length':1.0,'text':'Z'})
  call('changes.preview',{'id':plan['id'],'path':'Text.txt','beforeOffset':10**80,'length':4},True)
  preview=call('changes.preview',{'id':plan['id'],'path':'Text.txt','beforeOffset':0.0,'afterOffset':0.0,'length':4.0});assert preview['after']=='aZph'
  call('changes.apply',{'id':plan['id']});current=call('documents.readPage',{'path':'Text.txt'})
  call('documents.savePaged',{'path':'Text.txt','expectedRevision':current['revision']});assert source.read_bytes()==b'aZpha\nbeta\n'
  call('changes.undo',{'id':plan['id']});current=call('documents.readPage',{'path':'Text.txt'})
  call('documents.savePaged',{'path':'Text.txt','expectedRevision':current['revision']});assert source.read_bytes()==original
  call('documents.closePaged',{'path':'Text.txt','expectedRevision':current['revision']})
  host.stdin.close();assert host.wait(timeout=15)==0
  report.update(state='passed',checks=['huge-negative-fractional-offsets-refused','integral-json-floats-supported','paged-preview-apply-save-undo-save'],originalSourcePreserved=True,hostDrained=True)
 except BaseException as error:
  report.update(state='failed',error=str(error));raise
 finally:
  if host.poll() is None:host.terminate();host.wait(timeout=15)
  (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report),flush=True)

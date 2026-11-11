"""Opt-in real SDK/MCP capacity acceptance; source fixtures only, no test executable."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,stat,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
try:
 import psutil
except ImportError:
 psutil=None
P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','clangd','clang-tidy','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; no native process starts by default')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'checks':{}}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def mark(name,value=True):report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(65536),b''):h.update(block)
 return h.hexdigest()
def blake(data):return hashlib.blake2b(data,digest_size=32).hexdigest()
def decode(value):
 if value.isError:raise RuntimeError(value.content[0].text)
 return json.loads(value.content[0].text)
large=WORK/'Large.log';block=('// texte été 🛠\r\n'+'x'*1010+'\r\n').encode('utf-8');tail=b'TAIL\r\n'
with large.open('wb') as f:
 f.write(b'\xef\xbb\xbf')
 for _ in range((96*1024*1024)//len(block)+1):f.write(block)
 f.write(tail)
large_bytes=large.stat().st_size;large_hash=digest(large)
header='#pragma once\nint scaled_symbol(int input);\n'
cpp={'Tiny.cpp':'int tiny(int value) { return value+1; }\n'}
padding='/*'+'x'*(1280*1024)+'*/\n'
for i in range(8):cpp[f'Unit{i}.cpp']='#include "Scale.h"\n'+('int scaled_symbol(int input) { return input+1; }\n' if i==0 else f'int call_{i}() {{ return scaled_symbol({i}); }}\n')+padding
sources={'Scale.h':header,**cpp}
for name,text in sources.items():(WORK/name).write_text(text,encoding='utf-8',newline='\n')
baseline={name:digest(WORK/name) for name in sources}
items=''.join('<ClCompile Include="'+name+'"/>' for name in cpp)
(WORK/'Capacity.vcxproj').write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup><PropertyGroup Label="Globals"><ProjectGuid>{A9000304-6678-4D54-A08C-726385D5FF84}</ProjectGuid></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props"/><PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/><ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling></ClCompile></ItemDefinitionGroup><ItemGroup>'''+items+'''<ClInclude Include="Scale.h"/></ItemGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets"/></Project>''',encoding='utf-8')

def native_rss():
 if psutil is None:return 0
 values=[]
 for process in psutil.Process().children(recursive=True):
  try:
   if process.name().lower()==A.executable.name.lower():values.append(process.memory_info().rss)
  except (psutil.NoSuchProcess,psutil.AccessDenied):pass
 return sum(values)

async def main():
 params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
 with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
  async with stdio_client(params,errlog=log) as (read,write):
   async with ClientSession(read,write) as s:
    await s.initialize();tools=await s.list_tools();assert any(t.name=='op_documents_read_page' for t in tools.tools);mark('actualMcpDiscovery',len(tools.tools))
    async def call(name,args,fail=False,timeout=300):
     job=decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId'];deadline=time.monotonic()+timeout
     while time.monotonic()<deadline:
      result=decode(await s.call_tool('cy_get_job',{'jobId':job}))
      if result['state'] in ('Completed','Failed','Cancelled'):
       if fail:assert result['state']=='Failed',result;return result
       assert result['state']=='Completed',(name,result);return result['outputs']
      await asyncio.sleep(.02)
     raise TimeoutError(name)
    samples=[];sampling=True
    async def sample():
     while sampling:samples.append(native_rss());await asyncio.sleep(.05)
    before=native_rss();sampler=asyncio.create_task(sample())
    try:
     opened=await call('documents_read_page',{'path':'Large.log','length':29})
     assert opened['bytes']==large_bytes and opened['storage']=='paged' and len(opened['text'].encode('utf-8'))<=29
     assert (await call('documents_list',{}))['documents'][0]['storage']=='paged'
     emoji=3+block.index('🛠'.encode())
     await call('documents_read_page',{'path':'Large.log','offset':emoji+1,'expectedRevision':opened['revision']},fail=True)
     await call('documents_read_page',{'path':'Large.log','offset':opened['nextOffset']},fail=True)
     page=await call('documents_read_page',{'path':'Large.log','offset':opened['nextOffset'],'expectedRevision':opened['revision'],'length':29})
     with large.open('rb') as f:f.seek(page['offset']);assert page['text'].encode()==f.read(page['nextOffset']-page['offset'])
     await call('documents_read',{'path':'Large.log'},fail=True)
     start=large_bytes-len(tail)
     plan=await call('documents_prepare_byte_edit',{'path':'Large.log','expectedRevision':opened['revision'],'offset':start,'length':len(tail),'text':'RENAMED 🛠\r\n'})
     preview=await call('changes_preview',{'id':plan['id'],'path':'Large.log','beforeOffset':start,'afterOffset':start,'length':29})
     assert preview['before']=='TAIL\r\n' and preview['after']=='RENAMED 🛠\r\n' and preview['beforeBytes']>4*1024*1024
     await call('changes_apply',{'id':plan['id']});edited=await call('documents_read_page',{'path':'Large.log','offset':0})
     assert edited['dirty'];assert digest(large)==large_hash
     live=await call('search_text',{'query':'RENAMED','caseSensitive':True,'limit':5})
     assert any(row['path']=='Large.log' and 'RENAMED' in row['text'] for row in live['matches'])
     stale=await call('search_text',{'query':'TAIL','caseSensitive':True,'limit':5})
     assert not any(row['path']=='Large.log' for row in stale['matches'])
     pattern=await call('search_text',{'query':r'RENAMED \S+','caseSensitive':False,'regex':True,'limit':5})
     assert any(row['path']=='Large.log' for row in pattern['matches'])
     await call('documents_close_paged',{'path':'Large.log','expectedRevision':edited['revision']},fail=True)
     await call('documents_prepare_byte_edit',{'path':'Large.log','expectedRevision':opened['revision'],'offset':0,'length':0,'text':'x'},fail=True)
     await call('changes_undo',{'id':plan['id']});clean=await call('documents_read_page',{'path':'Large.log'})
     assert not clean['dirty'] and clean['bytes']==large_bytes
     # Apply/save, then Undo/save: original bytes are restored with explicit publication only.
     plan=await call('documents_prepare_byte_edit',{'path':'Large.log','expectedRevision':clean['revision'],'offset':start,'length':len(tail),'text':'RENAMED 🛠\r\n'})
     await call('changes_apply',{'id':plan['id']});edited=await call('documents_read_page',{'path':'Large.log'})
     saved=await call('documents_save_paged',{'path':'Large.log','expectedRevision':edited['revision']});assert not saved['dirty']
     with large.open('rb') as f:f.seek(start);assert f.read()=='RENAMED 🛠\r\n'.encode()
     await call('changes_undo',{'id':plan['id']});undone=await call('documents_read_page',{'path':'Large.log'});assert undone['dirty']
     await call('documents_save_paged',{'path':'Large.log','expectedRevision':undone['revision']});assert digest(large)==large_hash
     with large.open('ab') as f:f.write(b'EXTERNAL')
     conflict_hash=digest(large)
     await call('documents_save_paged',{'path':'Large.log','expectedRevision':undone['revision']},fail=True)
     assert digest(large)==conflict_hash
     with large.open('r+b') as f:f.truncate(large_bytes)
     await call('documents_prepare_byte_edit',{'path':'Large.log','expectedRevision':undone['revision'],'offset':emoji+1,'length':0,'text':'x'},fail=True)
     await call('documents_prepare_byte_edit',{'path':'Large.log','expectedRevision':undone['revision'],'offset':0,'length':0,'text':'\0'},fail=True)
     ro_plan=await call('documents_prepare_byte_edit',{'path':'Large.log','expectedRevision':undone['revision'],'offset':large_bytes,'length':0,'text':'changed'})
     await call('changes_apply',{'id':ro_plan['id']});readonly=await call('documents_read_page',{'path':'Large.log'})
     try:
      os.chmod(large,stat.S_IREAD)
      await call('documents_save_paged',{'path':'Large.log','expectedRevision':readonly['revision']},fail=True)
      assert digest(large)==large_hash
     finally:os.chmod(large,stat.S_IREAD|stat.S_IWRITE)
     await call('changes_undo',{'id':ro_plan['id']});restored=await call('documents_read_page',{'path':'Large.log'})
     await call('documents_close_paged',{'path':'Large.log','expectedRevision':restored['revision']})
     assert digest(large)==large_hash
    finally:sampling=False;await sampler
    peak=max(samples+[before]);assert psutil is None or peak-before < large_bytes//2,('Paged storage materialized a source-sized native allocation',before,peak,large_bytes)
    mark('largePagedText',{'sourceBytes':large_bytes,'memorySamplingAvailable':psutil is not None,'nativeRssBeforeBytes':before,'nativeRssPeakBytes':peak,'samples':len(samples),'scope':'Sampled production Core RSS only; no LSP running; may miss peaks','applyUndoSaveAndConflicts':True,'bomCrlfUnicodePreserved':True,'readOnlySaveRefused':True,'unsavedLiteralRegexGlobalSearch':True,'originalSourceRestored':True})
    tiny=await call('documents_read',{'path':'Tiny.cpp'});await call('documents_read_page',{'path':'Tiny.cpp'},fail=True)
    await call('documents_close',{'path':'Tiny.cpp','expectedRevision':tiny['revision']})
    prepared=await call('msbuild_prepare',{'entry':'Capacity.vcxproj','configuration':'Release','platform':'x64','tool':'msbuild'})
    assert prepared['exitCode']==0,prepared;db=WORK/prepared['project']['compilationDatabase']['path'];original=db.read_bytes()
    with db.open('wb') as f:
     f.write(b'[')
     for _ in range(70):f.write(b' '*1024*1024)
     f.write(original.lstrip()[1:])
    await call('language_configure',{'language':'cpp','enabled':True,'arguments':[str(A.clangd),'--background-index','-j=2','--log=error','--use-dirty-headers','--compile-commands-dir='+str(db.parent)]})
    inv=await call('analysis_prepare',{'scope':'compilation-database','persist':True,'batchSize':0})
    assert inv['total']==9;manifest_file=WORK/'.cycode/analysis-progress'/(inv['checkpoint']+'.json');manifest=json.loads(manifest_file.read_text('utf-8'));assert manifest['format']==2
    tiny=await call('documents_read',{'path':'Tiny.cpp'})
    inspected=await call('analysis_inspect',{'path':'Tiny.cpp','expectedRevision':tiny['revision'],'compilationDatabase':db.relative_to(WORK).as_posix(),'executable':str(A.clang_tidy),'profile':'recommended','timeoutSeconds':90})
    assert inspected['status']=='complete' and inspected['exitCode']==0,inspected
    await call('documents_close',{'path':'Tiny.cpp','expectedRevision':tiny['revision']})
    huge_cpp=sources['Tiny.cpp']+'/*'+'x'*(5*1024*1024)+'*/\n'
    (WORK/'Tiny.cpp').write_text(huge_cpp,encoding='utf-8',newline='\n')
    await asyncio.sleep(.5)
    huge=await call('documents_read_page',{'path':'Tiny.cpp'})
    huge_result=await call('analysis_inspect',{'path':'Tiny.cpp','expectedRevision':huge['revision'],'compilationDatabase':db.relative_to(WORK).as_posix(),'executable':str(A.clang_tidy),'profile':'recommended','timeoutSeconds':90})
    assert huge_result['status']=='complete' and huge_result['sourceWritten'] is False,huge_result
    header_edit=await call('documents_prepare_byte_edit',{'path':'Tiny.cpp','expectedRevision':huge['revision'],'offset':0,'length':0,'text':'// unsaved\n'})
    await call('changes_apply',{'id':header_edit['id']});dirty_cpp=await call('documents_read_page',{'path':'Tiny.cpp'})
    refused=await call('analysis_prepare',{'paths':['Scale.h']},fail=True);assert 'paged' in json.dumps(refused).lower()
    await call('analysis_inspect',{'path':'Tiny.cpp','expectedRevision':dirty_cpp['revision'],'compilationDatabase':db.relative_to(WORK).as_posix(),'executable':str(A.clang_tidy),'profile':'recommended','timeoutSeconds':90},fail=True)
    await call('changes_undo',{'id':header_edit['id']});huge=await call('documents_read_page',{'path':'Tiny.cpp'})
    await call('documents_close_paged',{'path':'Tiny.cpp','expectedRevision':huge['revision']})
    assert (WORK/'Tiny.cpp').read_text('utf-8')==huge_cpp
    (WORK/'Tiny.cpp').write_text(sources['Tiny.cpp'],encoding='utf-8',newline='\n')
    mark('largeCppDiskInspection',{'sourceBytes':len(huge_cpp.encode()),'status':huge_result['status'],'unsavedPagedCompilerQueriesRefused':True,'originalSourceRestored':True})
    mark('actualCompilerDatabaseAbove64MiB',{'bytes':db.stat().st_size,'realMsbuildCommands':9,'inventory':inv['total'],'inspectionStatus':inspected['status']})
    # Seeded cache DATA exercises aggregate size, not compiler observations. Every
    # row is unknown and must remain historical on restoration. No source proof.
    count=17000;identity='analysis-'+'f'*32;directory=manifest_file.parent/(identity+'.parts');directory.mkdir()
    seeded={**manifest,'checkpoint':identity,'total':count,'observed':count,'pathPages':[],'recordPages':[],
      'summary':{'commands':count,'translationUnits':count,'outOfScope':0,'observed':count,'remaining':0,'diagnosticClean':0,'diagnosticErrors':0,'diagnosticUnknown':count}}
    aggregate=0
    for offset in range(0,count,256):
     paths=[f'Historical/Unit{i:05}.cpp' for i in range(offset,min(count,offset+256))]
     records=[{'path':p,'revision':0,'diagnostics':'unknown','errors':0,'error':'fixture-only historical error '+'x'*2010} for p in paths]
     for kind,values,key in [('paths',paths,'pathPages'),('records',records,'recordPages')]:
      raw=json.dumps(values,separators=(',',':')).encode();aggregate+=len(raw);keyhash=blake(raw)
      (directory/(kind+'-'+keyhash+'.json')).write_bytes(raw);seeded[key].append(keyhash)
    (manifest_file.parent/(identity+'.json')).write_text(json.dumps(seeded),encoding='utf-8')
    assert aggregate>32*1024*1024
    restored=await call('analysis_prepare',{'checkpoint':identity,'batchSize':0})
    assert restored['completed']==count and restored['coverage']['diagnosticUnknown']==count and restored['proofReused'] is False
    finalpage=await call('analysis_results',{'session':restored['session'],'offset':count-1,'limit':1})
    assert finalpage['documents'][0]['evidence']=='historical-checkpoint' and finalpage['documents'][0]['diagnostics']=='unknown'
    # A modified immutable page must fail before any restored proof is issued.
    active_file=manifest_file.parent/(identity+'.json');active=json.loads(active_file.read_text('utf-8'))
    valid_manifest=active_file.read_bytes();nested=0
    for _ in range(20):nested=[nested]
    try:
     active_file.write_text(json.dumps({**active,'untrusted':nested}),encoding='utf-8')
     await call('analysis_prepare',{'checkpoint':identity,'batchSize':0},fail=True)
    finally:active_file.write_bytes(valid_manifest)
    payload=directory/('records-'+active['recordPages'][0]+'.json');good=payload.read_bytes()
    try:
     payload.write_bytes(b'{'+good[1:])
     await call('analysis_prepare',{'checkpoint':identity,'batchSize':0},fail=True)
    finally:payload.write_bytes(good)
    mark('checkpointAggregateAbove32MiB',{'seededDataBytes':aggregate,'rows':count,'compilerEvidence':False,'allRestoredUnknown':True,'lastPageAccessible':True,'tamperedPageRefused':True,'deepManifestRefused':True})
    # Real compiler rename whose before+after snapshots exceed the former 16 MiB.
    await call('analysis_prepare',{'paths':list(sources)})
    opened=await call('documents_read',{'path':'Scale.h'})
    position={'line':1,'character':4}
    plan=await call('language_rename',{'path':'Scale.h','expectedRevision':opened['revision'],'position':position,'newName':'scaled_symbol_updated'})
    assert len(plan['files'])==9,plan
    transaction_bytes=sum(len(sources[f['path']].encode())*2 for f in plan['files']);assert transaction_bytes>16*1024*1024
    await call('changes_preview',{'id':plan['id'],'path':'Unit0.cpp'},fail=True)
    page=await call('changes_preview',{'id':plan['id'],'path':'Unit0.cpp','beforeOffset':0,'afterOffset':0,'length':4096})
    assert 'scaled_symbol_updated' in page['after'] and page['hasMore']
    await call('changes_apply',{'id':plan['id']})
    for row in plan['files']:
     doc=await call('documents_read',{'path':row['path']});assert 'scaled_symbol_updated' in doc['text']
    await call('changes_undo',{'id':plan['id']})
    for row in plan['files']:assert (await call('documents_read',{'path':row['path']}))['text']==sources[row['path']]
    opened=await call('documents_read',{'path':'Scale.h'})
    stale=await call('language_rename',{'path':'Scale.h','expectedRevision':opened['revision'],'position':position,'newName':'scaled_symbol_updated'})
    last=await call('documents_read',{'path':'Unit7.cpp'});changed=await call('documents_replace',{'path':'Unit7.cpp','expectedRevision':last['revision'],'text':last['text']+'\n'})
    await call('changes_apply',{'id':stale['id']},fail=True)
    assert (await call('documents_read',{'path':'Unit0.cpp'}))['text']==sources['Unit0.cpp']
    await call('documents_replace',{'path':'Unit7.cpp','expectedRevision':changed['revision'],'text':sources['Unit7.cpp']})
    mark('actualCompilerTransactionAbove16MiB',{'beforeAfterBytes':transaction_bytes,'files':9,'previewApplyUndo':True,'stalePlanRefusedBeforeAnyMutation':True})
 assert not list((OUT/'jobs').glob('native-core-*'))
 assert digest(large)==large_hash and all(digest(WORK/name)==value for name,value in baseline.items())
 report.update(state='passed',originalSourcesPreserved=True,hostDrained=True);save()
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

"""Explicit production-CLI/MCP acceptance; no native test fixture is compiled."""
import argparse
import asyncio
from contextlib import AsyncExitStack
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from native_client import NativeClient
from native_sdk import NativeRuntime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    args = parser.parse_args()
    exe = args.executable.resolve(strict=True)
    fixture = args.fixture.absolute()
    fixture.mkdir(parents=True, exist_ok=False)
    workspace = fixture/'project'; workspace.mkdir()
    source = workspace/'main.cpp'
    baseline = '// été Σ\nint answer = 7;\n'
    source.write_bytes(baseline.encode('utf-8'))
    large = ('// SENSITIVE_VALIDATION_MARKER été Σ '+ 'x'*96 +'\n') * 10000
    (workspace/'large.txt').write_bytes(large.encode('utf-8'))
    descriptor = fixture/'native.json'
    environment = {**os.environ, 'CYCODE_CORE_EXECUTABLE': str(exe),
        'CYCODE_CORE_HOME': str(fixture/'home'), 'CYCODE_INDEX_DIR': str(fixture/'index'),
        'CYCODE_INDEX_AT_START': '1', 'GIT_OPTIONAL_LOCKS': '0'}
    os.environ.update({k: environment[k] for k in ('CYCODE_CORE_EXECUTABLE','CYCODE_CORE_HOME')})
    reports = []
    def checked(name, command, expected=0):
        result = subprocess.run(command, capture_output=True, encoding='utf-8', env=environment, timeout=60)
        assert result.returncode == expected, (name, result.stderr[:2000], result.stdout[:1000])
        if result.stdout: json.loads(result.stdout)
        reports.append(name)
        return json.loads(result.stdout) if result.stdout else json.loads(result.stderr)
    version = subprocess.run([str(exe),'--version'], capture_output=True, encoding='utf-8', env=environment, check=True)
    assert version.stdout.startswith('CyCode ')
    reports.append('legacy-version')
    checked('legacy-native-dispatch', [str(exe), str(workspace),'workspace.files','{}'])
    cli = [str(exe),'--sdk','--data-dir',str(fixture/'cli'),'--workspace',str(workspace)]
    checked('cli-operation-description', cli+['describe-operation','index_symbols'])
    checked('cli-indexed-search', cli+['invoke','search_text','{"query":"answer","engine":"index","waitMs":20000}'])
    checked('cli-read-only-denied', cli+['--local-access-profile','local-controlled','invoke','documents_replace',json.dumps({'path':'main.cpp','expectedRevision':1,'text':'denied'})],1)
    checked('cli-invalid-schema', cli+['invoke','documents_read','{"path":7}'],1)
    log=(fixture/'native.log').open('wb')
    native=subprocess.Popen([str(exe),'--disable-plugins',str(workspace),'--serve',str(descriptor)],stdin=subprocess.PIPE,stdout=log,stderr=log,env=environment)
    try:
        deadline=time.monotonic()+30
        while not descriptor.is_file():
            assert native.poll() is None and time.monotonic()<deadline, 'Native descriptor not ready'
            time.sleep(.05)
        direct=NativeClient(descriptor)
        async def scenario():
            from mcp import ClientSession, StdioServerParameters
            from mcp.client.stdio import stdio_client
            async with AsyncExitStack() as stack:
                async def client(name, rights='', profile='compact', extra=()):
                    params=StdioServerParameters(command=str(exe),args=['--sdk','--native-descriptor',str(descriptor),
                        '--data-dir',str(fixture/name),'--permissions',rights,'--mcp-profile',profile,*extra,'mcp'],env=environment)
                    read,write=await stack.enter_async_context(stdio_client(params))
                    session=await stack.enter_async_context(ClientSession(read,write))
                    await session.initialize()
                    return session
                reader=await client('reader')
                writer=await client('writer','workspace.write',extra=['--mcp-telemetry',str(fixture/'telemetry.jsonl'),
                    '--mcp-inline-bytes','4096','--mcp-spool',str(fixture/'spool'),'--mcp-result-ttl-seconds','60'])
                names={tool.name for tool in (await reader.list_tools()).tools}
                assert names=={'cy_tools_search','cy_tools_describe','cy_tools_call','cy_resources_read','cy_resources_grep'}, names
                reports.append('official-mcp-five-tool-discovery')
                async def tool(session,name,arguments,fail=False):
                    reply=await session.call_tool(name,arguments)
                    assert bool(reply.isError)==fail, (name,reply)
                    return json.loads(reply.content[0].text)
                async def domain(session,name,arguments=None,fail=False):
                    value=await tool(session,'cy_tools_call',{'name':name,'arguments':arguments or {}},fail)
                    if not fail and isinstance(value,dict) and 'resource' in value:
                        return await recover(session,value['resource']['uri'])
                    return value
                async def recover(session,uri):
                    content=await tool(session,'cy_resources_read',{'uri':uri})
                    text=content['contents'][0]['text']
                    try: manifest=json.loads(text)
                    except json.JSONDecodeError: manifest={}
                    if isinstance(manifest,dict) and manifest.get('type')=='cymcp.resource.v1':
                        chunks=[];uri=manifest['firstUri']
                        while uri:
                            part=await tool(session,'cy_resources_read',{'uri':uri})
                            chunks.append(part['contents'][0]['text'])
                            uri=part.get('_meta',{}).get('com.cymcp/chunk',{}).get('nextUri')
                        text=''.join(chunks)
                    return json.loads(text)
                discovered=await tool(reader,'cy_tools_search',{'query':'index_symbols'})
                assert any(row['name']=='index_symbols' for row in discovered['tools'])
                await tool(reader,'cy_tools_describe',{'name':'index_symbols'})
                status=await domain(reader,'service_status')
                assert Path(status['engine']['workspace']).resolve()==workspace.resolve()
                index=await domain(reader,'index_prepare',{'waitMs':20000})
                symbols=await domain(reader,'index_symbols',{'query':'answer','waitMs':20000})
                assert symbols['symbols'], symbols
                found=await domain(reader,'search_text',{'query':'answer','engine':'index','waitMs':20000})
                assert 'answer' in json.dumps(found)
                reports.append('mcp-real-index-and-native-engine')
                document=await domain(reader,'documents_read',{'path':'main.cpp'})
                update={'path':'main.cpp','expectedRevision':document['revision'],'text':'// MCP été Σ\nint answer = 19;\n'}
                denied=await domain(reader,'documents_replace',update,True)
                assert 'AccessDenied' in json.dumps(denied)
                edited=await domain(writer,'documents_replace',update)
                assert direct.call('documents.read',{'path':'main.cpp'})['text']==update['text']
                assert source.read_bytes()==baseline.encode('utf-8'), 'Implicit save'
                stale=await domain(writer,'documents_replace',update,True)
                assert 'Revision' in json.dumps(stale) or 'Stale' in json.dumps(stale) or 'Conflict' in json.dumps(stale), stale
                reports.append('live-instance-utf8-edit-denial-revision-and-no-save')
                await domain(reader,'documents_read_page',{'path':'bad\x00.cpp'},True)
                reports.append('full-canonical-pattern-enforcement')
                (workspace/'paged.cpp').write_bytes(b'int paged = 1;\n')
                page=await domain(writer,'documents_read_page',{'path':'paged.cpp'})
                plan=await domain(writer,'documents_prepare_byte_edit',{'path':'paged.cpp','offset':0,'length':0,
                    'text':'// inserted\n','expectedRevision':page['revision']})
                await domain(writer,'changes_apply',{'id':plan['id']})
                current=await domain(writer,'documents_read_page',{'path':'paged.cpp'})
                assert current['text'].startswith('// inserted')
                await domain(writer,'changes_undo',{'id':plan['id']})
                current=await domain(writer,'documents_read_page',{'path':'paged.cpp'})
                assert current['text']=='int paged = 1;\n'
                history=await domain(writer,'changes_history_status')
                redo=await domain(writer,'changes_history_prepare',{'id':history['redo']['id'],'direction':'redo',
                    'expectedHistoryRevision':history['revision'],'documents':[{'path':'paged.cpp',
                    'expectedDocumentId':current['id'],'expectedRevision':current['revision']}]})
                await domain(writer,'changes_apply',{'id':redo['id']})
                assert (await domain(writer,'documents_read_page',{'path':'paged.cpp'}))['text'].startswith('// inserted')
                assert (workspace/'paged.cpp').read_bytes()==b'int paged = 1;\n'
                reports.append('native-paged-preview-apply-undo-redo-no-save')
                large_reply=await tool(writer,'cy_tools_call',{'name':'documents_read','arguments':{'path':'large.txt'}})
                assert 'resource' in large_reply, str(large_reply)[:200]
                uri=large_reply['resource']['uri']
                restored=await recover(writer,uri)
                assert restored['text']==large
                await tool(reader,'cy_resources_read',{'uri':uri},True)
                assert any(p.is_file() for p in (fixture/'spool').rglob('*'))
                reports.append('complete-large-result-chunks-disk-spool-and-private-owner')
                legacy=await client('legacy','',profile='full')
                page = await legacy.list_tools()
                legacy_names={row.name for row in page.tools}
                while page.nextCursor:
                    page=await legacy.list_tools(cursor=page.nextCursor)
                    legacy_names.update(row.name for row in page.tools)
                assert 'op_documents_read' in legacy_names
                job=await tool(legacy,'op_documents_read',{'parameters':{'path':'main.cpp'}})
                deadline=time.monotonic()+15
                while job['state'] not in ('Completed','Failed','Cancelled'):
                    assert time.monotonic()<deadline
                    await asyncio.sleep(.03)
                    job=await tool(legacy,'cy_get_job',{'jobId':job['jobId']})
                assert job['state']=='Completed',job
                assert job['outputs']['text']==update['text']
                reports.append('legacy-op-job-contract-and-live-state')
                expiring=await client('expiring',extra=['--mcp-inline-bytes','4096','--mcp-result-ttl-seconds','1'])
                transient=await tool(expiring,'cy_tools_call',{'name':'documents_read','arguments':{'path':'large.txt'}})
                assert 'resource' in transient
                await asyncio.sleep(1.2)
                expired=await tool(expiring,'cy_resources_read',{'uri':transient['resource']['uri']},True)
                assert 'Resource' in json.dumps(expired)
                assert direct.call('documents.read',{'path':'main.cpp'})['text']==update['text']
                reports.append('expired-handle-rejects-without-action-replay')
                # A real CMake child, not a generated test executable. Cancel the
                # durable job and verify its worker is held until that child stops.
                (workspace/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(CancelBuild NONE)\n'
                    'file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/entered-configure" "ready")\n'
                    'execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 30)\n',encoding='utf-8')
                builder=await client('builder','process.execute')
                issued=await domain(builder,'core_job_submit',{'operationId':'build_configure',
                    'parameters':{'buildDirectory':'cancel-build','timeoutSeconds':60}})
                deadline=time.monotonic()+20
                while not (workspace/'entered-configure').exists():
                    assert time.monotonic()<deadline, await domain(builder,'core_job_get',{'jobId':issued['jobId']})
                    await asyncio.sleep(.1)
                await domain(builder,'core_job_cancel',{'jobId':issued['jobId']})
                deadline=time.monotonic()+15
                while True:
                    cancelled=await domain(builder,'core_job_get',{'jobId':issued['jobId']})
                    if cancelled['state'] in ('Completed','Failed','Cancelled'):break
                    assert time.monotonic()<deadline
                    await asyncio.sleep(.1)
                assert cancelled['state']=='Cancelled',cancelled
                assert not direct.call('build.status',{})['running']
                reports.append('real-build-job-cancellation-releases-native-worker')
        asyncio.run(scenario())
        runtime=NativeRuntime(fixture/'durable',executable=exe,native_descriptor=descriptor)
        try:
            runtime.start()
            credentials=runtime.create_client('writer',['workspace.write'])
            actor=runtime.authenticate(credentials['token'])
            sid=runtime.create_session(actor)['sessionId']
            job=runtime.submit(actor,sid,'documents_read',{'path':'main.cpp'})
            completed=runtime.wait(actor,job['jobId'])
            assert completed['state']=='Completed',completed
        finally: runtime.close()
        runtime=NativeRuntime(fixture/'durable',executable=exe,native_descriptor=descriptor)
        try:
            actor=runtime.authenticate(credentials['token'])
            assert runtime.get_job(actor,job['jobId'])['outputs']==completed['outputs']
            other=runtime.authenticate(runtime.create_client('other')['token'])
            try: runtime.get_job(other,job['jobId']); raise AssertionError('Cross-owner job access')
            except Exception as error: assert getattr(error,'code','')=='AccessDenied',error
        finally: runtime.close()
        reports.append('durable-job-identity-restart-and-owner-isolation')
        telemetry=(fixture/'telemetry.jsonl').read_text('utf-8')
        assert 'SENSITIVE_VALIDATION_MARKER' not in telemetry and 'MCP été' not in telemetry
        allowed={'stage','name','status','elapsed_ms','input_bytes','output_bytes','cache_hit','timestamp_ms'}
        assert all(set(json.loads(line))<=allowed for line in telemetry.splitlines())
        reports.append('metadata-only-telemetry')
        result={'platform':sys.platform,'sdkRevision':json.loads((APP/'cymcp-source.lock.json').read_text('utf-8'))['revision'],'checks':reports,
            'nativeFixtureExecutablesCompiled':False,'actionsUsed':False,'workspaceUnchangedOnDisk':source.read_bytes()==baseline.encode('utf-8')}
        (fixture/'report.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(json.dumps(result,indent=2))
    finally:
        if native.poll() is None: native.stdin.close();native.wait(timeout=30)
        log.close()

if __name__=='__main__':main()

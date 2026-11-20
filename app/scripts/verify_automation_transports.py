"""Explicit production HTTP/attach and legacy-data acceptance, with an external MCP client."""
import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import time
import urllib.request
APP=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(APP))
from native_client import NativeClient


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--executable',type=Path,required=True)
    p.add_argument('--fixture',type=Path,required=True)
    p.add_argument('--manager-bundle',type=Path,required=True)
    args=p.parse_args();exe=args.executable.resolve(strict=True)
    root=args.fixture.absolute();root.mkdir(parents=True,exist_ok=False)
    work=root/'workspace';work.mkdir();source=work/'main.cpp';source.write_text('int original = 1;\n',encoding='utf-8')
    home=root/'home';home.mkdir()
    shutil.copytree(args.manager_bundle/'tools',home/'tools')
    (home/'app/scripts').mkdir(parents=True)
    installed=home/exe.name
    shutil.copyfile(exe,installed)
    for name in ('CyTool.json','MCPMethods.json'):
        shutil.copyfile(APP/name,home/'app'/name)
    for name in ('migrate_legacy_jobs.py','install_core_agents.py'):
        shutil.copyfile(APP/'scripts'/name,home/'app/scripts'/name)
    env={**os.environ,'CYCODE_CORE_HOME':str(home),'CYCODE_INDEX_AT_START':'0'}
    processes=[];logs=[];checks=[]
    def spawn(arguments,name):
        log=(root/(name+'.log')).open('wb');logs.append(log)
        process=subprocess.Popen([str(exe),*arguments],stdin=subprocess.PIPE,stdout=log,stderr=log,env=env)
        processes.append(process);return process
    def ready(path,process):
        deadline=time.monotonic()+30
        while not path.is_file():
            assert process.poll() is None and time.monotonic()<deadline,'Service did not become ready'
            time.sleep(.05)
    native_path=root/'native.json'
    native=spawn(['--disable-plugins',str(work),'--serve',str(native_path)],'native')
    try:
        ready(native_path,native);direct=NativeClient(native_path)
        service_path=root/'service.json'
        service=spawn(['--sdk','--native-descriptor',str(native_path),'--data-dir',str(root/'service'),
            '--service-descriptor',str(service_path),'--local-access-profile','protected',
            '--mcp-notifications','--mcp-cache-ttl-ms','60000','serve'],'service')
        ready(service_path,service);d=json.loads(service_path.read_text('utf-8'))
        assert d['readUrl']!=d['editUrl'] and d['readToken']!=d['editToken']
        def wire(access,method,parameters,extra=None):
            body={'protocolVersion':'1.0','id':'acceptance','method':method,'params':parameters,**(extra or {})}
            request=urllib.request.Request(d[access+'Url'],json.dumps(body).encode(),
                headers={'Authorization':'Bearer '+d[access+'Token'],'Content-Type':'application/json'})
            with urllib.request.urlopen(request,timeout=30) as response:return json.load(response)
        original=wire('read','Invoke',{'operationId':'documents_read','parameters':{'path':'main.cpp'}})['result']
        update={'path':'main.cpp','text':'// live HTTP été\nint original = 2;\n','expectedRevision':original['revision']}
        denied=wire('read','Invoke',{'operationId':'documents_replace','parameters':update},{'principal':'local-owner'})
        assert denied['error']['code']=='AccessDenied',denied
        edited=wire('edit','Invoke',{'operationId':'documents_replace','parameters':update})
        assert edited['result']['text']==update['text']
        assert direct.call('documents.read',{'path':'main.cpp'})['text']==update['text']
        assert source.read_text('utf-8')=='int original = 1;\n'
        checks.append('separate-http-roles-real-live-state-no-principal-escalation')
        async def http_mcp():
            from mcp import ClientSession
            from mcp.client.streamable_http import streamablehttp_client
            async with streamablehttp_client(d['mcpReadUrl'],headers={'Authorization':'Bearer '+d['readToken']}) as (read,write,_):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    assert len((await session.list_tools()).tools)==5
                    reply=await session.call_tool('cy_tools_call',{'name':'documents_read','arguments':{'path':'main.cpp'}})
                    assert not reply.isError,reply
                    assert json.loads(reply.content[0].text)['text']==update['text']
                    denied=await session.call_tool('cy_tools_call',{'name':'documents_replace','arguments':update})
                    assert denied.isError
                    first=await session.read_resource('cycode://workspace/changes')
                    old=json.loads(first.contents[0].text)
                    live=direct.call('documents.read',{'path':'main.cpp'})
                    direct.call('documents.replace',{'path':'main.cpp','text':update['text']+'// changed\n','expectedRevision':live['revision']})
                    deadline=time.monotonic()+5
                    while time.monotonic()<deadline:
                        current=json.loads((await session.read_resource('cycode://workspace/changes')).contents[0].text)
                        if current!=old:break
                        await asyncio.sleep(.2)
                    assert current!=old,'Native resource did not update before its 60s TTL'
        asyncio.run(http_mcp());checks.append('official-streamable-http-mcp-and-native-invalidation-before-ttl')
        async def concurrent_proxies():
            from contextlib import AsyncExitStack
            from mcp import ClientSession, StdioServerParameters
            from mcp.client.stdio import stdio_client
            async with AsyncExitStack() as stack:
                clients=[]
                for _ in range(2):
                    params=StdioServerParameters(command=str(exe),args=['--sdk','--bridge-config',str(service_path),'mcp'],env=env)
                    read,write=await stack.enter_async_context(stdio_client(params))
                    session=await stack.enter_async_context(ClientSession(read,write));await session.initialize();clients.append(session)
                replies=await asyncio.gather(*(session.call_tool('cy_tools_call',{'name':'service_status','arguments':{}}) for session in clients))
                assert all(not reply.isError for reply in replies),replies
                assert all(Path(json.loads(reply.content[0].text)['engine']['workspace']).resolve()==work.resolve() for reply in replies)
        asyncio.run(concurrent_proxies());checks.append('concurrent-proxies-use-one-live-engine-with-independent-local-state')
        # Existing Python command shape proxies to the same Engine through the new service.
        compatibility=subprocess.run([sys.executable,'-B',str(APP/'tool.py'),'--core-executable',str(exe),
            '--bridge-config',str(service_path),'--access','read','--data-dir',str(root/'proxy'),
            'invoke','documents_read','--params','{"path":"main.cpp"}'],capture_output=True,encoding='utf-8',env=env,timeout=30)
        assert compatibility.returncode==0,compatibility.stderr
        assert json.loads(compatibility.stdout)['state']=='Completed'
        checks.append('old-python-command-proxies-to-shared-native-http-service')
        # Old database is loaded once by the native installed-helper path, with no old SDK.
        state=root/'legacy';state.mkdir();database=state/'state.sqlite3';token='legacy-fixture-token'
        db=sqlite3.connect(database);db.execute('CREATE TABLE records(kind TEXT,id TEXT,value TEXT,PRIMARY KEY(kind,id))')
        rows=[('clients','old-client',{'clientId':'old-client','name':'legacy','permissions':[],
            'tokenHash':hashlib.sha256(token.encode()).hexdigest()}),
            ('jobs','old-result',{'jobId':'old-result','clientId':'old-client','sessionId':'old-task','state':'Completed',
                'operationId':'documents_read','parameters':{},'outputs':{'text':'retained output'},'error':None}),
            ('jobs','old-running',{'jobId':'old-running','clientId':'old-client','sessionId':'old-task','state':'Running',
                'operationId':'documents_replace','parameters':{'text':'MUST NEVER REPLAY'},'outputs':{},'error':None})]
        for kind,id,row in rows:db.execute('INSERT INTO records VALUES(?,?,?)',(kind,id,json.dumps(row)))
        db.commit();db.close();before=database.read_bytes()
        request=''.join(json.dumps({'id':id,'method':'GetJob','params':{'jobId':id},'authToken':token})+'\n' for id in ('old-result','old-running'))
        converted=subprocess.run([str(installed),'--sdk','--native-descriptor',str(native_path),'--data-dir',str(state),'stdio'],
            input=request,encoding='utf-8',capture_output=True,env=env,timeout=30)
        assert converted.returncode==0,converted.stderr
        replies=[json.loads(line) for line in converted.stdout.splitlines()]
        assert replies[0]['result']['outputs']['text']=='retained output',replies
        assert replies[1]['result']['state']=='Failed' and replies[1]['result']['error']['code']=='Interrupted'
        assert database.read_bytes()==before
        assert 'MUST NEVER REPLAY' not in direct.call('documents.read',{'path':'main.cpp'})['text']
        checks.append('native-auto-import-old-credentials-results-interrupted-no-replay')
        result={'platform':sys.platform,'checks':checks,'actionsUsed':False,'sourceUnchangedOnDisk':source.read_text('utf-8')=='int original = 1;\n'}
        (root/'report.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(json.dumps(result,indent=2))
    finally:
        if native.poll() is None:native.stdin.close();native.wait(timeout=30)
        for process in processes:
            if process.poll() is None:
                try:process.wait(timeout=20)
                except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
        for log in logs:log.close()

if __name__=='__main__':main()

"""Real DAP through official Core MCP, using the production Core as debuggee."""
from pathlib import Path
import argparse,asyncio,json,os,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser();P.add_argument('--core',type=Path,required=True);P.add_argument('--executable',type=Path,required=True);P.add_argument('--adapter',type=Path,required=True);P.add_argument('--output',type=Path,required=True)
P.add_argument('--run-native',action='store_true')
A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; no Core is launched by default')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
report={'state':'running','scope':'Production Core --version as debuggee, installed CodeLLDB, actual SDK/MCP; no generated executable','checks':{}}
def save():(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def mark(name,value=True):report['checks'][name]=value;save();print(json.dumps({'stage':name,'value':value}),flush=True)
def decode(result):
    if result.isError:raise RuntimeError(result.content[0].text)
    return json.loads(result.content[0].text)
async def main():
    params=StdioServerParameters(command=sys.executable,args=['-B','-X','utf8',str(A.core/'app/tool.py'),'--core-workspace',str(WORK),'--core-executable',str(A.executable),'--disable-native-plugins','--data-dir',str(OUT/'jobs'),'mcp'],env={**os.environ,'CYCODE_PERMISSIONS':'workspace.write,process.execute','PYTHONDONTWRITEBYTECODE':'1'})
    with (OUT/'stderr.log').open('w',encoding='utf-8') as log:
      async with stdio_client(params,errlog=log) as (read,write):
       async with ClientSession(read,write) as session:
        await session.initialize()
        async def call(name,args,expect_failure=False):
            job=decode(await session.call_tool('op_'+name,{'parameters':args}))['jobId'];until=time.monotonic()+90
            while time.monotonic()<until:
                value=decode(await session.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):
                    if expect_failure:assert value['state']=='Failed',value;return value
                    if value['state']!='Completed':raise RuntimeError(name+': '+json.dumps(value))
                    return value['outputs']
                await asyncio.sleep(.02)
            raise TimeoutError(name)
        try:
            await call('debug_launch',{'program':str(A.executable),'adapter':str(A.adapter),'adapterId':'builtin','runtime':'native','args':['--version'],'cwd':'.','stopOnEntry':True,'terminal':'console','buildBeforeLaunch':False})
            until=time.monotonic()+30
            while time.monotonic()<until:
                status=await call('debug_status',{})
                if status['state']=='stopped':break
                if status.get('error'):raise RuntimeError(status['error'])
                await asyncio.sleep(.1)
            (OUT/'stopped.json').write_text(json.dumps(status,indent=2)+'\n',encoding='utf-8')
            assert status['state']=='stopped' and status['frames'],status
            mark('actualStopAndStack',len(status['frames']))
            reference=next((f.get('instructionPointerReference') for f in status['frames'] if f.get('instructionPointerReference')),None)
            if reference and status['capabilities'].get('supportsDisassembleRequest'):
                result=await call('debug_disassemble',{'memoryReference':reference,'instructionCount':8,'stopGeneration':status['stopGeneration']})
                assert result['body'].get('instructions');mark('actualDisassembly',len(result['body']['instructions']))
            if reference and status['capabilities'].get('supportsReadMemoryRequest'):
                result=await call('debug_memory',{'memoryReference':reference,'count':16,'stopGeneration':status['stopGeneration']})
                assert result['body'].get('data');mark('actualMemoryRead')
            if status['scopes']:
                scope=next((s for s in status['scopes'] if s.get('variablesReference')),None)
                if scope:
                    result=await call('debug_variables',{'variablesReference':scope['variablesReference'],'count':20,'stopGeneration':status['stopGeneration']})
                    mark('actualScopeRead',len(result['variables']))
            await call('debug_memory',{'memoryReference':reference or '0','count':16,'stopGeneration':status['stopGeneration']+1},expect_failure=True);mark('staleStopRejected')
            await call('debug_continue',{'sessionGeneration':status['sessionGeneration'],'stopGeneration':status['stopGeneration'],'threadId':status['threadId']})
            until=time.monotonic()+30
            while time.monotonic()<until:
                end=await call('debug_status',{})
                if end['state']=='exited':break
                if end.get('error'):raise RuntimeError(end['error'])
                await asyncio.sleep(.1)
            assert end['state']=='exited',end;mark('actualContinueAndExit')
        finally:
            end=await call('debug_stop',{});assert end['state']=='idle';mark('ownedDebuggerStopped')
    assert not list((OUT/'jobs').glob('native-core-*'));mark('hostDrained');report['state']='passed';save()
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

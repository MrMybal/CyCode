"""Actual DAP on the production Core: conditional functions, threads and exceptions."""
from pathlib import Path
import argparse,asyncio,hashlib,json,os,subprocess,sys,time
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
P=argparse.ArgumentParser(description=__doc__)
for name in ('core','executable','debuggee','adapter','output'):P.add_argument('--'+name,type=Path,required=True)
P.add_argument('--run-native',action='store_true');A=P.parse_args()
if not A.run_native:P.error('Explicit --run-native is required; only existing production binaries are used')
OUT=A.output.resolve();OUT.mkdir(exist_ok=False);WORK=OUT/'workspace';WORK.mkdir()
report={'state':'running','nativeSha256':hashlib.sha256(A.executable.read_bytes()).hexdigest(),'debuggeeSha256':hashlib.sha256(A.debuggee.read_bytes()).hexdigest(),'checks':{},'unsupported':[]}
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
            job=decode(await s.call_tool('op_'+name,{'parameters':args}))['jobId'];until=time.monotonic()+120
            while time.monotonic()<until:
                value=decode(await s.call_tool('cy_get_job',{'jobId':job}))
                if value['state'] in ('Completed','Failed','Cancelled'):
                    if fail:assert value['state']=='Failed',value;return value
                    assert value['state']=='Completed',(name,value);return value['outputs']
                await asyncio.sleep(.02)
            raise TimeoutError(name)
        async def state(wanted,after=None):
            until=time.monotonic()+60
            while time.monotonic()<until:
                v=await call('debug_status',{})
                inspected=v['state']!='stopped' or (v.get('threads') and v.get('frames'))
                if v['state'] in wanted and inspected and (after is None or v.get('stopGeneration',0)>after):return v
                if v.get('error'):raise RuntimeError(v['error'])
                await asyncio.sleep(.05)
            raise TimeoutError(json.dumps(v))
        def context(v):return {k:v[k] for k in ('sessionGeneration','stopGeneration','threadId')}
        base={'program':str(A.debuggee),'adapter':str(A.adapter),'adapterId':'builtin','runtime':'native','cwd':'.','terminal':'integrated','buildBeforeLaunch':False}
        try:
            await call('debug_launch',{**base,'args':['--version'],'stopOnEntry':True})
            stopped=await state(['stopped']);caps=stopped['capabilities'];(OUT/'capabilities.json').write_text(json.dumps(caps,indent=2)+'\n',encoding='utf-8')
            assert caps.get('supportsFunctionBreakpoints') and caps.get('supportsConditionalBreakpoints'),caps
            await call('debug_function_breakpoints',{'breakpoints':[{'name':'main','condition':'argc > 0'}]})
            await call('debug_stop',{})
            await call('debug_launch',{**base,'args':['--disable-plugins',str(WORK),'--stdio'],'stopOnEntry':False})
            stopped=await state(['stopped']);assert stopped['reason']=='breakpoint',stopped
            evaluated=await call('debug_evaluate',{'expression':'argc','context':'watch'})
            assert '4' in str(evaluated),evaluated
            mark('actualConditionalFunctionBreakpoint',{'reason':stopped['reason'],'argcEvaluated':True})
            reference=next((f.get('instructionPointerReference') for f in stopped['frames'] if f.get('instructionPointerReference')),None)
            if reference and caps.get('supportsInstructionBreakpoints'):
                current=await call('debug_status',{})
                points=await call('debug_instruction_breakpoints',{'sessionGeneration':current['sessionGeneration'],'stopGeneration':current['stopGeneration'],'expectedRevision':current['instructionBreakpointRevision'],'breakpoints':[{'instructionReference':reference}]})
                assert points['instructionBreakpointsSynchronized'] and len(points['instructionBreakpoints'])==1,points
                assert points['instructionBreakpoints'][0]['verification']['verified'],points
                await call('debug_instruction_breakpoints',{'sessionGeneration':points['sessionGeneration'],'stopGeneration':points['stopGeneration'],'expectedRevision':points['instructionBreakpointRevision'],'breakpoints':[]})
                stopped=await state(['stopped'])
                mark('actualInstructionBreakpointBindingAndClear')
            await call('debug_function_breakpoints',{'breakpoints':[]})
            await call('debug_continue',context(stopped));running=await state(['running'])
            # Give the genuine Engine time to start its language/recovery/watcher workers.
            await asyncio.sleep(2)
            await call('debug_pause',{});stopped=await state(['stopped'],after=stopped['stopGeneration'])
            assert len(stopped['threads'])>=2,stopped
            selected=stopped['threadId'];other=next(t['id'] for t in stopped['threads'] if t['id']!=selected)
            await call('debug_thread',{'id':other});stopped=await state(['stopped']);assert stopped['threadId']==other
            assert stopped['frames'];await call('debug_frame',{'id':stopped['frames'][0]['id']})
            mark('actualMultithreadPauseAndSelection',{'threads':len(stopped['threads']),'selectedOtherThread':True,'stackAndFrameRead':True})
            stopped=await call('debug_status',{})
            frame=stopped['frames'][0];reference=frame.get('instructionPointerReference')
            if reference and caps.get('supportsDisassembleRequest'):
                result=await call('debug_disassemble',{'memoryReference':reference,'instructionCount':8,'stopGeneration':stopped['stopGeneration']})
                assert result['body'].get('instructions');mark('actualStoppedThreadDisassembly',len(result['body']['instructions']))
            if reference and caps.get('supportsReadMemoryRequest'):
                result=await call('debug_memory',{'memoryReference':reference,'count':16,'stopGeneration':stopped['stopGeneration']});assert result['body'].get('data');mark('actualStoppedThreadMemory')
            stale={**context(stopped),'stopGeneration':stopped['stopGeneration']+1}
            await call('debug_continue',stale,True);mark('staleExecutionRefused')
            await call('debug_stop',{})
            filters=caps.get('exceptionBreakpointFilters',[])
            throw_filter=next((r['filter'] for r in filters if 'throw' in r.get('filter','').lower()),None)
            if throw_filter:
                await call('debug_exception_breakpoints',{'filters':[throw_filter]})
                await call('debug_launch',{**base,'args':['--disable-plugins',str(WORK),'method.doesNotExist'],'stopOnEntry':False})
                exception=await state(['stopped','exited'])
                verification=exception.get('exceptionBreakpointStatus')
                assert verification and verification['acknowledged'],exception
                mark('actualExceptionConfigurationEvidence',verification)
                if exception['state']=='stopped' and exception['reason']=='exception':
                    if caps.get('supportsExceptionInfoRequest'):
                        info=await call('debug_exception_info',{'threadId':exception['threadId'],'stopGeneration':exception['stopGeneration']});mark('actualExceptionInfo',bool(info))
                    mark('actualCppThrowBreakpoint',throw_filter)
                else:
                    report['unsupported'].append('CodeLLDB advertised cpp_throw but did not stop this MSVC production throw; no native exception-filter acceptance claim')
                    await call('debug_stop',{})
                    # Explicit MSVC runtime recipe, separate from DAP exception-filter support.
                    await call('debug_function_breakpoints',{'breakpoints':[{'name':'_CxxThrowException'}]})
                    await call('debug_launch',{**base,'args':['--disable-plugins',str(WORK),'method.doesNotExist'],'stopOnEntry':False})
                    throw=await state(['stopped','exited'])
                    assert throw['state']=='stopped' and throw['reason']=='breakpoint',throw
                    assert any('CxxThrowException' in f.get('name','') for f in throw['frames']),throw
                    mark('actualMsvcThrowFunctionBreakpoint',{'scope':'Explicit _CxxThrowException function breakpoint; not DAP exceptionInfo or automatic fallback','stackObserved':True})
            else:report['unsupported'].append('Adapter advertised no throw filter; no exception-stop claim')
            await call('debug_stop',{})
            target=None
            with (OUT/'attached-target-stderr.log').open('wb') as errors:
                try:
                    target=subprocess.Popen([str(A.debuggee),'--disable-plugins',str(WORK),'--stdio'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=errors,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                    ready=json.loads(await asyncio.wait_for(asyncio.to_thread(target.stdout.readline),60));assert ready['event']=='ready'
                    await call('debug_attach',{'pid':target.pid,'adapter':str(A.adapter),'adapterId':'builtin','runtime':'native','cwd':'.'})
                    attached=await state(['running','stopped'])
                    if attached['state']=='running':await call('debug_pause',{});attached=await state(['stopped'])
                    assert attached['attached'] and len(attached['threads'])>=2,attached
                    await call('debug_stop',{});assert target.poll() is None,'Detach terminated the independently started target'
                    request={'protocol':'cycode-core/1','id':'after-detach','method':'service.status','params':{}}
                    target.stdin.write((json.dumps(request)+'\n').encode());target.stdin.flush()
                    answer=json.loads(await asyncio.wait_for(asyncio.to_thread(target.stdout.readline),20))
                    assert answer['id']=='after-detach' and answer['result']['service']['state']=='ready',answer
                    mark('actualLocalAttachAndDetachPreservesTarget',{'threads':len(attached['threads']),'postDetachNativeRequestSucceeded':True,'scope':'Same Windows host, existing production CLI; not a remote-machine test'})
                    request.update(id='shutdown',method='service.shutdown')
                    target.stdin.write((json.dumps(request)+'\n').encode());target.stdin.flush()
                    await asyncio.wait_for(asyncio.to_thread(target.stdout.readline),20)
                    target.stdin.close();await asyncio.to_thread(target.wait,30);assert target.returncode==0
                finally:
                    if target and target.poll() is None:target.terminate();await asyncio.to_thread(target.wait,15)
        finally:
            end=await call('debug_stop',{});assert end['state']=='idle';mark('ownedDebuggerStopped')
    assert not list((OUT/'jobs').glob('native-core-*'));report['state']='passed';mark('hostDrained')
try:asyncio.run(main())
except BaseException as error:report.update(state='failed',error=str(error));save();raise

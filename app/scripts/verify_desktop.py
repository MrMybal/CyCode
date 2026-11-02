"""Verify the real GUI, its automatically started Core and official MCP client."""
import asyncio
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def wait_job(session, submitted):
    if submitted.isError:
        raise RuntimeError(submitted.content[0].text)
    job = json.loads(submitted.content[0].text)
    for _ in range(150):
        reply = await session.call_tool('cy_get_job', {'jobId':job['jobId']})
        done = json.loads(reply.content[0].text)
        if done['state'] == 'Completed':
            return done['outputs']
        if done['state'] in ('Failed','Cancelled'):
            raise RuntimeError(json.dumps(done))
        await asyncio.sleep(.03)
    raise TimeoutError('Desktop MCP job did not finish')

def main():
    with tempfile.TemporaryDirectory(prefix='cycode-desktop-') as temporary:
        workspace = Path(temporary)
        original = 'int main() { return 0; }\n'
        source = workspace/'main.cpp'
        source.write_text(original, encoding='utf-8')
        capture = ROOT/'data/reports/desktop-mcp-live.bmp'
        executable = ROOT/('CyCode.exe' if sys.platform == 'win32' else 'CyCode')
        startup = None
        if sys.platform == 'win32':
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        process = subprocess.Popen([str(executable), '--workspace',str(workspace),'--open','main.cpp',
            '--test-frames','240','--test-capture',str(capture)],cwd=workspace.parent,startupinfo=startup)
        descriptor = None
        try:
            deadline = time.monotonic()+10
            while descriptor is None and time.monotonic()<deadline:
                for candidate in (ROOT/'data/bridge').glob('native-*.json'):
                    try:
                        native = json.loads(candidate.read_text('utf-8'))
                        if Path(native['workspace']).resolve()==workspace.resolve():
                            core = candidate.with_name(candidate.name.replace('native-','core-',1))
                            if core.exists():
                                descriptor=core
                    except (OSError,ValueError,KeyError):
                        continue
                time.sleep(.03)
            if descriptor is None:
                raise RuntimeError('GUI did not start its shared Core service')
            async def scenario():
                parameters = StdioServerParameters(command=sys.executable,args=[str(ROOT/'app/tool.py'),
                    '--bridge-config',str(descriptor),'--access','edit','mcp'])
                async with stdio_client(parameters) as (read,write):
                    async with ClientSession(read,write) as session:
                        await session.initialize()
                        document=await wait_job(session,await session.call_tool('op_documents_read',{'parameters':{'path':'main.cpp'}}))
                        text='// MCP_LIVE_BUFFER: edited through the real CyToolsCore service\nint answer = 31;\nint main() { return answer; }\n'
                        result=await wait_job(session,await session.call_tool('op_documents_replace',{'parameters':{
                            'path':'main.cpp','text':text,'expectedRevision':document['revision']}}))
                        if not result['dirty'] or source.read_text('utf-8')!=original:
                            raise RuntimeError('Live edit unexpectedly saved or was not applied')
                        position={'line':2, 'character':text.splitlines()[2].index('answer')}
                        inputs={'path':'main.cpp','position':position,'expectedRevision':result['revision']}
                        definition=await wait_job(session,await session.call_tool('op_language_definition',{'parameters':inputs}))
                        if definition['locations'][0]['range']['start'] != {'line':1,'character':4}:
                            raise RuntimeError(f'Desktop shared clangd definition failed: {definition}')
                        references=await wait_job(session,await session.call_tool('op_language_references',{'parameters':inputs}))
                        if len(references['locations']) != 2:
                            raise RuntimeError(f'Desktop shared clangd references failed: {references}')
                        stats=await wait_job(session,await session.call_tool('op_plugins_invoke',{'parameters':{
                            'command':'sample.text_statistics','input':{'text':text}}}))
                        if stats['words']<5:
                            raise RuntimeError('GUI native plugin did not execute')
            asyncio.run(scenario())
            process.wait(timeout=25)
            if process.returncode or not capture.exists():
                raise RuntimeError('Desktop render or shutdown failed')
            if descriptor.exists():
                raise RuntimeError('Shared Core descriptor survived GUI shutdown')
            print('Desktop acceptance passed: GUI startup from another cwd, automatic shared Core, official MCP live edit, shared clangd definition/references, native plugin, capture and shutdown.')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)

if __name__ == '__main__':
    main()

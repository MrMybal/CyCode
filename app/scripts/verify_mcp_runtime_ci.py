"""Explicit Windows runner acceptance of the installed, isolated official MCP runtime."""
import argparse
import asyncio
import contextlib
import importlib.metadata
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation',type=Path,required=True)
    parser.add_argument('--fixture-root',type=Path,required=True)
    parser.add_argument('--reports',type=Path,required=True)
    args=parser.parse_args()
    if sys.platform!='win32' or os.environ.get('GITHUB_ACTIONS')!='true':
        parser.error('Installed MCP acceptance is restricted to an explicit Windows GitHub Actions job.')
    installation=args.installation.resolve(strict=True);parent=args.fixture_root.resolve(strict=True)
    if installation.parent!=parent or not installation.name.startswith('CyCode-'):
        parser.error('Use the owned installation created by installer acceptance.')
    interpreter=installation/'tools/python/python.exe'
    if Path(sys.executable).resolve()!=interpreter or not sys.flags.isolated or not sys.flags.utf8_mode or not sys.dont_write_bytecode:
        parser.error('Run this check with the installed isolated interpreter, -B and -X utf8.')
    reports=args.reports.absolute()
    if reports.parent.resolve()!=parent.parent/'reports':parser.error('Keep reports in this runner checkout.')
    reports.mkdir(parents=True,exist_ok=True)
    lock=json.loads((installation/'app/mcp-runtime.lock.json').read_text('utf-8'))
    assert sys.version_info[:3]==tuple(map(int,lock['python']['version'].split('.')))
    packages=installation/'tools/python/Lib/site-packages'
    for row in lock['wheels']:
        dist=importlib.metadata.distribution(row['project'])
        assert dist.version==row['version'],row['project']
        assert Path(dist.locate_file('')).resolve()==packages,row['project']
    # _pth isolation deliberately omits the script directory. Add only this
    # verified first-party package directory for the data-only bundle verifier.
    sys.path.insert(0,str(installation/'app/scripts'))
    # Import only after the runner/installation guards, from the actual package.
    import tool
    from mcp import ClientSession,StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp_bundle import load_bundle
    assert Path(tool.__file__).resolve()==installation/'app/tool.py'
    load_bundle(installation,lock)
    manifest=json.loads((installation/'app/CyTool.json').read_text('utf-8'))
    expected={'op_'+entry['id'] for entry in manifest['operations']}
    fixture=Path(tempfile.mkdtemp(prefix='mcp-owned-',dir=parent))
    workspace=fixture/'workspace-été';workspace.mkdir()
    source=workspace/'main.cpp';baseline='// été Σ\nint answer = 1;\n';source.write_bytes(baseline.encode('utf-8'))
    native_descriptor=fixture/'native.json';core_descriptor=fixture/'core.json'
    logs=[];processes=[]
    def spawn(command,name):
        log=(fixture/name).open('wb');logs.append(log)
        process=subprocess.Popen(command,cwd=workspace,stdin=subprocess.PIPE,stdout=log,stderr=subprocess.STDOUT)
        processes.append(process);return process
    def wait_ready(path,process):
        end=time.monotonic()+25
        while not path.is_file():
            if process.poll() is not None or time.monotonic()>end:
                raise RuntimeError('Packaged native/Core service did not start; private fixture logs retained.')
            time.sleep(.025)
    native=None;service=None
    try:
        native=spawn([str(installation/'cycode_cli.exe'),str(workspace),'--serve',str(native_descriptor)],'native.log')
        wait_ready(native_descriptor,native)
        service=spawn([str(interpreter),'-B','-X','utf8',str(installation/'app/tool.py'),'--desktop-service',
            '--native-descriptor',str(native_descriptor),'--service-descriptor',str(core_descriptor),
            '--data-dir',str(fixture/'core-jobs')],'core.log')
        wait_ready(core_descriptor,service)
        client=tool.NativeClient(native_descriptor)
        async def scenario():
            def connection(access):
                return StdioServerParameters(command=str(interpreter),args=['-B','-X','utf8',
                    str(installation/'app/tool.py'),'--bridge-config',str(core_descriptor),'--access',access,'mcp'])
            async def job(session,operation,parameters,success=True):
                reply=await session.call_tool('op_'+operation,{'parameters':parameters})
                assert not reply.isError,'Valid packaged MCP job was rejected.'
                identifier=json.loads(reply.content[0].text)['jobId']
                end=time.monotonic()+20
                while time.monotonic()<end:
                    answer=await session.call_tool('cy_get_job',{'jobId':identifier})
                    assert not answer.isError
                    value=json.loads(answer.content[0].text)
                    if value['state'] in {'Completed','Failed','Cancelled'}:
                        assert value['state']==('Completed' if success else 'Failed'),value.get('error',{})
                        return identifier,value
                    await asyncio.sleep(.025)
                raise RuntimeError('Packaged MCP job did not settle.')
            async with stdio_client(connection('read')) as (ar,aw),stdio_client(connection('edit')) as (br,bw):
                async with ClientSession(ar,aw) as reader,ClientSession(br,bw) as writer:
                    await reader.initialize();await writer.initialize()
                    names={entry.name for entry in (await reader.list_tools()).tools}
                    assert expected<=names,'Installed manifest operations are missing from the official MCP catalogue.'
                    _,original=await job(reader,'documents_read',{'path':'main.cpp'})
                    doc=original['outputs'];assert doc['text']==baseline
                    invalid=await reader.call_tool('op_documents_read',{'parameters':{}})
                    assert invalid.isError,'MCP input validation was bypassed.'
                    denied=await reader.call_tool('op_documents_replace',{'parameters':{
                        'path':'main.cpp','expectedRevision':doc['revision'],'text':'must not be written'}})
                    assert denied.isError,'Read-only MCP identity acquired write access.'
                    changed='// été Σ\nint answer = 99;\n'
                    identifier,updated=await job(writer,'documents_replace',{
                        'path':'main.cpp','expectedRevision':doc['revision'],'text':changed})
                    hidden=await reader.call_tool('cy_get_job',{'jobId':identifier})
                    assert hidden.isError,'Read-only client could inspect another identity private job.'
                    actual=client.call('documents.read',{'path':'main.cpp'})
                    assert actual['text']==changed and actual['dirty'] and actual['id']==doc['id']
                    _,visible=await job(reader,'documents_read',{'path':'main.cpp'})
                    assert visible['outputs']['text']==changed
                    await job(writer,'documents_replace',{
                        'path':'main.cpp','expectedRevision':doc['revision'],'text':'stale text'},success=False)
                    assert client.call('documents.read',{'path':'main.cpp'})['text']==changed
                    _,plugins=await job(reader,'plugins_list',{})
                    packaged={json.loads(p.read_text('utf-8'))['id'] for p in (installation/'extensions').glob('*.json')}
                    active={p['id'] for p in plugins['outputs']['plugins'] if p.get('active') and p.get('loaded')}
                    assert packaged<=active,'A bundled native plugin failed to load in the installed host.'
                    assert source.read_bytes()==baseline.encode('utf-8'),'MCP editing unexpectedly saved source bytes.'
        asyncio.run(scenario())
        native.stdin.close();native.wait(timeout=20)
        assert native.returncode==0 and not native_descriptor.exists()
        service.wait(timeout=25)
        assert service.returncode==0 and not core_descriptor.exists(),'Shared Core did not follow native shutdown.'
        load_bundle(installation,lock)
        (reports/'result.json').write_text(json.dumps({'version':manifest['version'],
            'python':lock['python']['version'],'sdk':lock['sdk']['version'],'mcp':'1.30.0',
            'operations':len(expected),'isolatedImports':True,'utf8':True,'sharedCore':True,
            'readOnlyPermission':True,'privateJobIsolation':True,'nativeLiveEdit':True,
            'staleRevisionRefused':True,'bundledPluginsLoaded':True,'sourceBytesPreserved':True,
            'nativeAndCoreShutdown':True,'managedRuntimeUnchanged':True},indent=2)+'\n','utf-8')
        print('Installed official MCP: catalogue, shared Core identities, Unicode live edits, guarded revisions, plugins and shutdown passed.')
    finally:
        if native is not None and native.poll() is None and native.stdin:
            with contextlib.suppress(OSError):native.stdin.close()
        for process in processes:
            if process.poll() is None:
                try:process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:process.wait(timeout=10)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        for log in logs:log.close()

if __name__=='__main__':main()

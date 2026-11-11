"""Runner-only native viewports and simultaneous Engine isolation, without an SDK stub."""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    args=parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS')!='true':
        parser.error('Native viewport acceptance requires an explicit GitHub Actions runner.')
    executable=args.executable.resolve(strict=True)
    if not executable.is_relative_to(ROOT/'runtime'):
        parser.error('Use this checkout production executable.')
    parent=ROOT/'runtime/test-workspaces/viewports-ci';parent.mkdir(parents=True,exist_ok=True)
    reports=ROOT/'runtime/reports/viewports-ci';reports.mkdir(parents=True,exist_ok=True)
    fixture=Path(tempfile.mkdtemp(prefix='owned-',dir=parent)).resolve()
    processes=[];streams=[];endpoints=[]
    def endpoint(workspace,process):
        deadline=time.monotonic()+20
        while process.poll() is None and time.monotonic()<deadline:
            for descriptor in (ROOT/'data/bridge').glob('native-*.json'):
                try:
                    if descriptor.stat().st_size>8192:continue
                    value=json.loads(descriptor.read_text('utf-8'))
                    if Path(value['workspace']).resolve()!=workspace:continue
                    assert value['protocol']=='cycode-native/1' and value['host']=='127.0.0.1'
                    assert type(value['port']) is int and 1<=value['port']<=65535
                    assert isinstance(value['token'],str) and len(value['token'])==64
                    return descriptor,value
                except (OSError,ValueError,KeyError):continue
            time.sleep(.025)
        raise RuntimeError('Owned native bridge did not become ready; inspect the instance log.')
    def call(value,method,parameters):
        # Exercise the real loopback bridge; this client has no document model,
        # scheduler, MCP implementation or replacement CyToolsCore identity.
        request={'token':value['token'],'method':method,'params':parameters}
        with socket.create_connection(('127.0.0.1',value['port']),timeout=5) as connection:
            connection.sendall((json.dumps(request,ensure_ascii=False)+'\n').encode('utf-8'))
            response=bytearray()
            while b'\n' not in response:
                chunk=connection.recv(65536)
                if not chunk:raise RuntimeError('Owned bridge disconnected during isolation acceptance.')
                response.extend(chunk)
                if len(response)>1024*1024:raise RuntimeError('Unexpectedly large isolation response.')
        answer=json.loads(response)
        if 'error' in answer:raise RuntimeError(answer['error']['message'])
        return answer['result']
    try:
        workspaces=[]
        for index,frames in enumerate((1000,2400)):
            workspace=fixture/('instance-'+str(index)+'-été');workspace.mkdir();workspaces.append(workspace)
            (workspace/'main.cpp').write_bytes(('int answer = '+str(index+1)+';\n').encode('utf-8'))
            stream=(reports/('instance-'+str(index)+'.log')).open('wb');streams.append(stream)
            report=reports/('instance-'+str(index)+'.json')
            process=subprocess.Popen([str(executable),'--workspace',str(workspace),'--open','main.cpp',
                '--language','en','--theme','graphite','--disable-plugins','--test-windows',
                '--test-frames',str(frames),'--test-report',str(report)],cwd=workspace,
                stdout=stream,stderr=subprocess.STDOUT)
            processes.append(process);endpoints.append(endpoint(workspace,process))
        assert processes[0].poll() is None and processes[1].poll() is None,'Instances did not overlap.'
        assert endpoints[0][0]!=endpoints[1][0] and endpoints[0][1]['port']!=endpoints[1][1]['port']
        before=call(endpoints[0][1],'documents.read',{'path':'main.cpp'})
        call(endpoints[0][1],'documents.replace',{'path':'main.cpp','text':'int answer = 99;\n',
            'expectedRevision':before['revision']})
        assert call(endpoints[1][1],'documents.read',{'path':'main.cpp'})['text']=='int answer = 2;\n'
        processes[0].wait(timeout=75)
        assert processes[0].returncode==0 and processes[1].poll() is None,'Shutdown crossed an instance boundary.'
        assert call(endpoints[1][1],'documents.read',{'path':'main.cpp'})['text']=='int answer = 2;\n'
        processes[1].wait(timeout=90)
        assert processes[1].returncode==0
        for index,workspace in enumerate(workspaces):
            state=json.loads((reports/('instance-'+str(index)+'.json')).read_text('utf-8'))
            assert not state['error'],state['error']
            assert state['active']=='main.cpp' and state['text']==('int answer = 99;\n' if index==0 else 'int answer = 2;\n')
            assert state['dirty']==(index==0),'The rendered live document did not match its isolated native owner.'
            stages={entry['stage']:entry for entry in state['windowTest']}
            assert stages['detached']['panelNative'] and stages['detached']['editorNative'],stages
            assert stages['detached']['nativeWindows']==3,stages
            assert stages['closed']['panelHidden'] and stages['closed']['editorDocked'],stages
            assert stages['restored']['nativeWindows']==1 and stages['restored']['panelVisible'] and stages['restored']['panelDocked'],stages
            assert (workspace/'main.cpp').read_bytes()==('int answer = '+str(index+1)+';\n').encode('utf-8')
            assert not endpoints[index][0].exists(),'Owned native descriptor survived teardown.'
        (reports/'result.json').write_text(json.dumps({'nativeEditorAndPanelWindows':True,
            'closeReopenRedock':True,'simultaneousInstances':True,'liveBufferIsolation':True,
            'independentShutdown':True,'sourcePreservation':True,'sdkMcpAcceptance':False},indent=2)+'\n','utf-8')
        assert fixture.parent==parent.resolve() and not fixture.is_symlink() and fixture.resolve()==fixture
        shutil.rmtree(fixture)
        print('Actual native viewport lifecycle and two-instance Engine/bridge isolation passed. SDK/MCP acceptance is separate.')
    finally:
        for process in processes:
            if process.poll() is None:
                # These are only processes created above on the explicit runner.
                process.terminate()
                try:process.wait(timeout=10)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        for stream in streams:stream.close()

if __name__=='__main__':main()

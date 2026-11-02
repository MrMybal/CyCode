"""Real native executable, CyTools runtime and official MCP SDK integration."""
import asyncio
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from tool import NativeClient, create_runtime

class NativeIntegration(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='cycode-mcp-')
        self.directory = Path(self.temporary.name)
        self.workspace = self.directory / 'workspace été test'
        self.workspace.mkdir()
        (self.workspace / 'main.cpp').write_text('int main() { return 0; }\n', encoding='utf-8')
        self.descriptor = self.directory / 'bridge' / 'native.json'
        executable = Path(os.environ['CYCODE_TEST_CLI']).resolve()
        self.process = subprocess.Popen([str(executable), str(self.workspace), '--serve', str(self.descriptor)],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 10
        while not self.descriptor.exists():
            if self.process.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('Native bridge did not start')
            time.sleep(.03)
        self.previous = os.environ.get('CYCODE_BRIDGE')
        os.environ['CYCODE_BRIDGE'] = str(self.descriptor)
        self.client = NativeClient(self.descriptor)

    def tearDown(self):
        if self.previous is None:
            os.environ.pop('CYCODE_BRIDGE', None)
        else:
            os.environ['CYCODE_BRIDGE'] = self.previous
        self.process.communicate(b'\n' if self.process.poll() is None else None, timeout=10)
        self.temporary.cleanup()

    def test_live_document_and_permissions(self):
        runtime = create_runtime(self.directory / 'jobs')
        runtime.start()
        try:
            actor = runtime.authenticate(runtime.create_client('writer', ['workspace.write'])['token'])
            reader = runtime.authenticate(runtime.create_client('reader')['token'])
            session = runtime.create_session(actor, temporary=True)['sessionId']
            original = self.client.call('documents.read', {'path': 'main.cpp'})
            parameters = {'path': 'main.cpp', 'text': 'int main() { return 73; }\n', 'expectedRevision': original['revision']}
            job = runtime.submit(actor, session, 'documents_replace', parameters)
            done = runtime.wait(actor, job['jobId'], timeout=10)
            self.assertEqual(done['state'], 'Completed', done)
            self.assertIn('73', self.client.call('documents.read', {'path': 'main.cpp'})['text'])
            self.assertIn('return 0', (self.workspace / 'main.cpp').read_text('utf-8'))
            reader_session = runtime.create_session(reader, temporary=True)['sessionId']
            with self.assertRaises(Exception):
                runtime.submit(reader, reader_session, 'documents_replace', parameters)
            with self.assertRaises(Exception):
                runtime.get_job(reader, job['jobId'])
            stale = runtime.submit(actor, session, 'documents_replace', parameters)
            self.assertEqual(runtime.wait(actor, stale['jobId'], timeout=10)['state'], 'Failed')
        finally:
            runtime.close()
        self.assertTrue((self.workspace / 'main.cpp').exists())

    def test_official_mcp_roundtrip(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        async def scenario():
            parameters = StdioServerParameters(command=sys.executable,
                args=[str(APP / 'tool.py'), '--data-dir', str(self.directory / 'mcp-jobs'), 'mcp'],
                env={**os.environ, 'CYCODE_BRIDGE': str(self.descriptor), 'CYCODE_PERMISSIONS': 'workspace.write'})
            async with stdio_client(parameters) as (read, write):
                async with ClientSession(read, write) as session:
                    await session.initialize()
                    tools = await session.list_tools()
                    self.assertIn('op_search_text', [item.name for item in tools.tools])
                    submitted = await session.call_tool('op_documents_read', {'parameters': {'path': 'main.cpp'}})
                    self.assertFalse(submitted.isError)
                    job = json.loads(submitted.content[0].text)
                    for _ in range(100):
                        response = await session.call_tool('cy_get_job', {'jobId': job['jobId']})
                        done = json.loads(response.content[0].text)
                        if done['state'] in ('Completed', 'Failed', 'Cancelled'):
                            break
                        await asyncio.sleep(.03)
                    self.assertEqual(done['state'], 'Completed', done)
                    self.assertIn('int main()', done['outputs']['text'])
                    invalid = await session.call_tool('op_documents_read', {'parameters': {}})
                    self.assertTrue(invalid.isError)
        asyncio.run(scenario())

    def test_semantic_navigation_over_read_only_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        original = self.client.call('documents.read', {'path': 'main.cpp'})
        text = 'int answer = 7;\nint main() { const char* s = "🛠"; return answer + answer; }\n'
        live = self.client.call('documents.replace', {'path': 'main.cpp', 'text': text,
                                                     'expectedRevision': original['revision']})
        position = {'line': 1, 'character': len(text.splitlines()[1].split('answer')[0].encode('utf-16-le')) // 2}
        async def scenario():
            parameters = StdioServerParameters(command=sys.executable,
                args=[str(APP / 'tool.py'), '--data-dir', str(self.directory / 'semantic-jobs'), 'mcp'],
                env={**os.environ, 'CYCODE_BRIDGE': str(self.descriptor), 'CYCODE_PERMISSIONS': ''})
            async with stdio_client(parameters) as (read, write):
                async with ClientSession(read, write) as session:
                    await session.initialize()
                    names = [item.name for item in (await session.list_tools()).tools]
                    for name in ('op_language_definition', 'op_language_references', 'op_language_status'):
                        self.assertIn(name, names)
                    async def job(operation, inputs):
                        submitted = await session.call_tool(operation, {'parameters': inputs})
                        self.assertFalse(submitted.isError, submitted)
                        identifier = json.loads(submitted.content[0].text)['jobId']
                        deadline = time.monotonic() + 20
                        while time.monotonic() < deadline:
                            response = await session.call_tool('cy_get_job', {'jobId': identifier})
                            done = json.loads(response.content[0].text)
                            if done['state'] in ('Completed', 'Failed', 'Cancelled'):
                                return done
                            await asyncio.sleep(.03)
                        raise TimeoutError('Semantic MCP job did not finish')
                    inputs = {'path': 'main.cpp', 'position': position, 'expectedRevision': live['revision']}
                    definition = await job('op_language_definition', inputs)
                    self.assertEqual(definition['state'], 'Completed', definition)
                    locations = definition['outputs']['locations']
                    self.assertEqual(locations[0]['path'], 'main.cpp')
                    self.assertEqual(locations[0]['range']['start'], {'line': 0, 'character': 4})
                    references = await job('op_language_references', {**inputs, 'includeDeclaration': False})
                    self.assertEqual(references['state'], 'Completed', references)
                    self.assertEqual(len(references['outputs']['locations']), 2)
                    self.assertEqual(references['outputs']['positionEncoding'], 'utf-16')
                    status = await job('op_language_status', {})
                    self.assertTrue(status['outputs']['ready'])
                    stale = await job('op_language_definition', {**inputs, 'expectedRevision': original['revision']})
                    self.assertEqual(stale['state'], 'Failed', stale)
                    invalid = await session.call_tool('op_language_definition', {'parameters': {**inputs, 'position': {'line': -1, 'character': 0}}})
                    self.assertTrue(invalid.isError)
                    forbidden = await session.call_tool('op_documents_replace', {'parameters': {
                        'path': 'main.cpp', 'expectedRevision': live['revision'], 'text': ''}})
                    self.assertTrue(forbidden.isError)
        asyncio.run(scenario())
        self.assertEqual((self.workspace / 'main.cpp').read_text('utf-8'), original['text'])

    def test_cmake_toolchains_over_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        (self.workspace / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(McpBuild LANGUAGES CXX)\nadd_executable(demo main.cpp)\ntarget_compile_definitions(demo PRIVATE MCP_CMAKE_ACTIVE=1)\n', encoding='utf-8')
        async def scenario():
            parameters = StdioServerParameters(command=sys.executable,
                args=[str(APP / 'tool.py'), '--data-dir', str(self.directory / 'cmake-jobs'), 'mcp'],
                env={**os.environ, 'CYCODE_BRIDGE': str(self.descriptor), 'CYCODE_PERMISSIONS': 'process.execute'})
            async with stdio_client(parameters) as (read, write):
                async with ClientSession(read, write) as session:
                    await session.initialize()
                    async def job(operation, inputs):
                        submitted = await session.call_tool(operation, {'parameters': inputs})
                        self.assertFalse(submitted.isError, submitted)
                        identifier = json.loads(submitted.content[0].text)['jobId']
                        deadline = time.monotonic() + 40
                        while time.monotonic() < deadline:
                            reply = await session.call_tool('cy_get_job', {'jobId': identifier})
                            done = json.loads(reply.content[0].text)
                            if done['state'] in ('Completed', 'Failed', 'Cancelled'):
                                self.assertEqual(done['state'], 'Completed', done)
                                return done['outputs']
                            await asyncio.sleep(.03)
                        raise TimeoutError('CMake MCP job did not finish')
                    tools = await job('op_toolchains_detect', {})
                    self.assertTrue(tools['cmake'])
                    project = await job('op_build_inspect', {})
                    self.assertFalse(project['configured'])
                    configured = await job('op_build_configure', {'configuration': 'Release'})
                    self.assertEqual(configured['exitCode'], 0, configured)
                    self.assertTrue(configured['project']['compilationDatabase']['available'])
                    built = await job('op_build_run', {'target': 'demo'})
                    self.assertEqual(built['exitCode'], 0, built)
                    inspected = await job('op_build_inspect', {})
                    self.assertEqual(inspected['profile']['target'], 'demo')
                    self.assertIn('demo', [target['name'] for target in inspected['targets']])
                    invalid = await session.call_tool('op_build_configure', {'parameters': {'configuration': 'unknown'}})
                    self.assertTrue(invalid.isError)
        asyncio.run(scenario())
        runtime = create_runtime(self.directory / 'cmake-reader')
        runtime.start()
        try:
            reader = runtime.authenticate(runtime.create_client('reader')['token'])
            session = runtime.create_session(reader, temporary=True)['sessionId']
            with self.assertRaises(Exception):
                runtime.submit(reader, session, 'build_configure', {})
            inspected = runtime.submit(reader, session, 'build_inspect', {})
            self.assertEqual(runtime.wait(reader, inspected['jobId'], timeout=10)['state'], 'Completed')
        finally:
            runtime.close()

    def test_unreal_metadata_and_permissions_over_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        (self.workspace / 'Demo.uproject').write_text('{"FileVersion":3,"EngineAssociation":"uninstalled-test-engine","Modules":[{"Name":"Demo","Type":"Runtime"}]}', encoding='utf-8')
        source = self.workspace / 'Source'
        source.mkdir()
        (source / 'DemoEditor.Target.cs').write_text('// target rules\n', encoding='utf-8')
        (source / 'Demo.Build.cs').write_text('// module rules\n', encoding='utf-8')
        async def scenario():
            parameters = StdioServerParameters(command=sys.executable,
                args=[str(APP / 'tool.py'), '--data-dir', str(self.directory / 'unreal-jobs'), 'mcp'],
                env={**os.environ, 'CYCODE_BRIDGE': str(self.descriptor), 'CYCODE_PERMISSIONS': ''})
            async with stdio_client(parameters) as (read, write):
                async with ClientSession(read, write) as session:
                    await session.initialize()
                    tools = {t.name for t in (await session.list_tools()).tools}
                    for operation in ('inspect', 'generate', 'database', 'build', 'prepare'):
                        self.assertIn('op_unreal_' + operation, tools)
                    submitted = await session.call_tool('op_unreal_inspect', {'parameters': {}})
                    self.assertFalse(submitted.isError, submitted)
                    identifier = json.loads(submitted.content[0].text)['jobId']
                    for _ in range(200):
                        reply = await session.call_tool('cy_get_job', {'jobId': identifier})
                        job = json.loads(reply.content[0].text)
                        if job['state'] in ('Completed', 'Failed', 'Cancelled'):
                            break
                        await asyncio.sleep(.03)
                    self.assertEqual(job['state'], 'Completed', job)
                    project = job['outputs']
                    self.assertTrue(project['available'])
                    self.assertEqual(project['profile']['target'], 'DemoEditor')
                    self.assertEqual(project['modules'][0]['name'], 'Demo')
                    for operation in ('generate', 'database', 'build', 'prepare'):
                        denied = await session.call_tool('op_unreal_' + operation, {'parameters': {}})
                        self.assertTrue(denied.isError, denied)
                    invalid = await session.call_tool('op_unreal_inspect', {'parameters': {'configuration': 'Release'}})
                    self.assertTrue(invalid.isError)
        asyncio.run(scenario())
        self.assertFalse((self.workspace / 'Intermediate').exists())
        self.assertFalse((self.workspace / '.cycode').exists())

    def test_refactoring_preview_apply_and_permissions_over_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        original = 'int  FormatMe( ){return  7;}\n'
        (self.workspace / 'format.cpp').write_text(original, encoding='utf-8')
        (self.workspace / '.clang-format').write_text('BasedOnStyle: LLVM\nIndentWidth: 4\nAllowShortFunctionsOnASingleLine: None\n', encoding='utf-8')
        doc = self.client.call('documents.read', {'path': 'format.cpp'})
        async def scenario():
            def parameters(name, permissions):
                return StdioServerParameters(command=sys.executable,
                    args=[str(APP / 'tool.py'), '--data-dir', str(self.directory / name), 'mcp'],
                    env={**os.environ, 'CYCODE_BRIDGE': str(self.descriptor), 'CYCODE_PERMISSIONS': permissions})
            async def job(session, operation, inputs):
                submitted = await session.call_tool('op_' + operation, {'parameters': inputs})
                self.assertFalse(submitted.isError, submitted)
                identifier = json.loads(submitted.content[0].text)['jobId']
                for _ in range(600):
                    reply = await session.call_tool('cy_get_job', {'jobId': identifier})
                    done = json.loads(reply.content[0].text)
                    if done['state'] in ('Completed', 'Failed', 'Cancelled'): break
                    await asyncio.sleep(.03)
                self.assertEqual(done['state'], 'Completed', done)
                return done['outputs']
            async with stdio_client(parameters('refactor-reader', '')) as (ar, aw), stdio_client(parameters('refactor-writer', 'workspace.write')) as (br, bw):
                async with ClientSession(ar, aw) as reader, ClientSession(br, bw) as writer:
                    await reader.initialize(); await writer.initialize()
                    tools = {tool.name for tool in (await reader.list_tools()).tools}
                    for name in ('language_symbols', 'language_document_symbols', 'sources_read', 'language_format', 'language_rename', 'changes_preview', 'changes_apply', 'changes_undo'):
                        self.assertIn('op_' + name, tools)
                    outline = await job(reader, 'language_document_symbols', {'path': 'format.cpp', 'expectedRevision': doc['revision']})
                    self.assertTrue(any(s['name'] == 'FormatMe' for s in outline['symbols']), outline)
                    plan = await job(reader, 'language_format', {'path': 'format.cpp', 'expectedRevision': doc['revision']})
                    self.assertGreater(plan['editCount'], 0)
                    preview = await job(reader, 'changes_preview', {'id': plan['id'], 'path': 'format.cpp'})
                    self.assertEqual(preview['before'], original)
                    self.assertIn('\n    return 7;', preview['after'])
                    denied = await reader.call_tool('op_changes_apply', {'parameters': {'id': plan['id']}})
                    self.assertTrue(denied.isError, denied)
                    self.assertEqual(self.client.call('documents.read', {'path': 'format.cpp'})['text'], original)
                    applied = await job(writer, 'changes_apply', {'id': plan['id']})
                    self.assertEqual(applied['state'], 'applied')
                    live = self.client.call('documents.read', {'path': 'format.cpp'})
                    self.assertEqual(live['text'], preview['after'])
                    self.assertTrue(live['dirty'])
                    self.assertEqual((self.workspace / 'format.cpp').read_text('utf-8'), original)
                    await job(writer, 'changes_undo', {'id': plan['id']})
                    live = self.client.call('documents.read', {'path': 'format.cpp'})
                    renamed = await job(reader, 'language_rename', {'path': 'format.cpp', 'expectedRevision': live['revision'], 'position': {'line': 0, 'character': original.index('FormatMe')}, 'newName': 'RenamedFunction'})
                    self.assertEqual(renamed['state'], 'preview')
                    denied = await reader.call_tool('op_changes_apply', {'parameters': {'id': renamed['id']}})
                    self.assertTrue(denied.isError)
                    bad_source = await reader.call_tool('op_sources_read', {'parameters': {'source': str(self.workspace.parent / 'private.h')}})
                    self.assertTrue(bad_source.isError, bad_source)
        asyncio.run(scenario())
        self.assertEqual((self.workspace / 'format.cpp').read_text('utf-8'), original)

    def test_debugger_over_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        source='int main() {\n    int local = 41;\n    local += 1;\n    return local == 42 ? 0 : 1;\n}\n'
        (self.workspace/'main.cpp').write_text(source,'utf-8')
        (self.workspace/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(DebugMcp LANGUAGES CXX)\nadd_executable(debug_mcp main.cpp)\n','utf-8')
        configured=self.client.call('build.configure',{'configuration':'Debug'})
        self.assertEqual(configured['exitCode'],0,configured)
        built=self.client.call('build.run',{})
        self.assertEqual(built['exitCode'],0,built)
        target=next(t for t in built['project']['targets'] if t['name']=='debug_mcp')
        program=self.workspace/built['project']['profile']['buildDirectory']/target['artifacts'][0]['path']
        self.assertTrue(program.is_file(),program)
        async def scenario():
            def parameters(name,permissions):
                return StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/name),'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
            async def job(session,name,inputs):
                submitted=await session.call_tool('op_'+name,{'parameters':inputs})
                self.assertFalse(submitted.isError,submitted)
                identifier=json.loads(submitted.content[0].text)['jobId']
                for _ in range(800):
                    response=await session.call_tool('cy_get_job',{'jobId':identifier})
                    done=json.loads(response.content[0].text)
                    if done['state'] in ('Completed','Failed','Cancelled'): break
                    await asyncio.sleep(.02)
                self.assertEqual(done['state'],'Completed',done)
                return done['outputs']
            async with stdio_client(parameters('debug-reader','')) as (ar,aw), stdio_client(parameters('debug-writer','process.execute,workspace.write')) as (br,bw):
                async with ClientSession(ar,aw) as reader,ClientSession(br,bw) as writer:
                    await reader.initialize(); await writer.initialize()
                    tools={tool.name for tool in (await writer.list_tools()).tools}
                    for operation in ('debug_profile','debug_configure','debug_status','debug_breakpoints','debug_launch','debug_attach','debug_run','debug_continue','debug_pause','debug_next','debug_step_in','debug_step_out','debug_stop','debug_evaluate','debug_variables','debug_source'):
                        self.assertIn('op_'+operation,tools)
                    denied=await reader.call_tool('op_debug_status',{'parameters':{}})
                    self.assertTrue(denied.isError,denied)
                    profile=await job(writer,'debug_configure',{'program':str(program),'terminal':'integrated','watches':['local + 1']})
                    self.assertEqual(profile['program'],str(program))
                    await job(writer,'debug_breakpoints',{'path':'main.cpp','breakpoints':[{'line':3}]})
                    await job(writer,'debug_launch',{})
                    try:
                        for _ in range(50):
                            status=await job(writer,'debug_status',{})
                            if status['state']=='stopped' and status.get('watches'): break
                            await asyncio.sleep(.03)
                        self.assertEqual(status['state'],'stopped',status)
                        self.assertTrue(any(v['name']=='local' and v['value']=='41' for v in status['variables']),status)
                        evaluated=await job(writer,'debug_evaluate',{'expression':'local + 1'})
                        self.assertIn('42',evaluated['result'])
                        self.assertTrue(any('42' in w.get('result','') for w in status['watches']),status['watches'])
                        await job(writer,'debug_continue',{})
                        for _ in range(50):
                            status=await job(writer,'debug_status',{})
                            if status['state']=='exited': break
                            await asyncio.sleep(.03)
                        self.assertEqual(status['state'],'exited',status)
                    finally:
                        stopped=await job(writer,'debug_stop',{})
                        self.assertEqual(stopped['state'],'idle')
        asyncio.run(scenario())
        self.assertEqual((self.workspace/'main.cpp').read_text('utf-8'),source)

    def test_terminal_sessions_over_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        async def scenario():
            def parameters(name, permissions):
                return StdioServerParameters(command=sys.executable,
                    args=[str(APP/'tool.py'), '--data-dir', str(self.directory/name), 'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
            async def job(session, name, inputs):
                response=await session.call_tool('op_'+name,{'parameters':inputs})
                self.assertFalse(response.isError,response)
                identifier=json.loads(response.content[0].text)['jobId']
                for _ in range(400):
                    response=await session.call_tool('cy_get_job',{'jobId':identifier})
                    result=json.loads(response.content[0].text)
                    if result['state'] in ('Completed','Failed','Cancelled'): break
                    await asyncio.sleep(.02)
                self.assertEqual(result['state'],'Completed',result)
                return result['outputs']
            async with stdio_client(parameters('terminal-reader','')) as (ar,aw), stdio_client(parameters('terminal-owner','process.execute')) as (br,bw):
                async with ClientSession(ar,aw) as reader, ClientSession(br,bw) as writer:
                    await reader.initialize(); await writer.initialize()
                    denied=await reader.call_tool('op_terminal_create',{'parameters':{}})
                    self.assertTrue(denied.isError,denied)
                    denied=await reader.call_tool('op_terminal_list',{'parameters':{}})
                    self.assertTrue(denied.isError,denied)
                    created=await job(writer,'terminal_create',{'profile':'cmd' if os.name=='nt' else 'default'})
                    identifier=created['id']
                    try:
                        listed=await job(writer,'terminal_list',{})
                        self.assertTrue(any(s['id']==identifier for s in listed['sessions']))
                        await job(writer,'terminal_resize',{'id':identifier,'columns':115,'rows':20})
                        command='set /a McpAnswer=6*7\recho MCP_%McpAnswer%\r' if os.name=='nt' else "printf 'MCP_%s\\n' $((6*7))\r"
                        await job(writer,'terminal_write',{'id':identifier,'data':command})
                        for _ in range(40):
                            screen=await job(writer,'terminal_read',{'id':identifier})
                            if any('MCP_42' in line for line in screen['lines']): break
                            await asyncio.sleep(.05)
                        self.assertTrue(any('MCP_42' in line for line in screen['lines']),screen)
                        self.assertEqual(screen['columns'],115)
                    finally:
                        await job(writer,'terminal_close',{'id':identifier})
                    self.assertEqual((await job(writer,'terminal_list',{}))['sessions'],[])
        asyncio.run(scenario())

    def test_git_workflows_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        def git(*args):
            return subprocess.run(['git','-C',str(self.workspace),'-c','user.name=CyCode test','-c','user.email=cycode@example.invalid','-c','commit.gpgsign=false',*args],capture_output=True,check=True)
        git('init','--initial-branch=main'); git('add','.'); git('commit','-m','Initial')
        async def scenario():
            def parameters(name, permissions):
                return StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/name),'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
            async def job(session,name,inputs,expected='Completed'):
                response=await session.call_tool('op_'+name,{'parameters':inputs})
                self.assertFalse(response.isError,response)
                identifier=json.loads(response.content[0].text)['jobId']
                for _ in range(300):
                    response=await session.call_tool('cy_get_job',{'jobId':identifier})
                    result=json.loads(response.content[0].text)
                    if result['state'] in ('Completed','Failed','Cancelled'): break
                    await asyncio.sleep(.02)
                self.assertEqual(result['state'],expected,result)
                return result.get('outputs',{})
            async with stdio_client(parameters('git-reader','')) as (ar,aw), stdio_client(parameters('git-owner','vcs.write,workspace.write,process.execute')) as (br,bw):
                async with ClientSession(ar,aw) as reader, ClientSession(br,bw) as writer:
                    await reader.initialize(); await writer.initialize()
                    denied=await reader.call_tool('op_vcs_branch_create',{'parameters':{'name':'denied'}})
                    self.assertTrue(denied.isError,denied)
                    metadata=await job(reader,'vcs_inspect',{})
                    self.assertEqual(metadata['branch'],'main')
                    self.assertEqual(len((await job(reader,'vcs_history',{}))['commits']),1)
                    original=await job(reader,'documents_read',{'path':'main.cpp'})
                    await job(writer,'vcs_branch_create',{'name':'feature'})
                    await job(writer,'vcs_branch_switch',{'name':'feature'})
                    edit=await job(writer,'documents_replace',{'path':'main.cpp','text':'int main() { return 88; }\n','expectedRevision':original['revision']})
                    await job(writer,'vcs_branch_switch',{'name':'main'},'Failed')
                    await job(writer,'documents_save',{'path':'main.cpp','expectedRevision':edit['revision']})
                    stash=await job(writer,'vcs_stash_save',{'message':'MCP stash'})
                    self.assertIn('main.cpp',stash['reloaded'])
                    restored=await job(reader,'documents_read',{'path':'main.cpp'})
                    self.assertIn('return 0',restored['text'])
                    await job(writer,'vcs_branch_switch',{'name':'main'})
                    await job(writer,'vcs_stash_pop',{})
                    self.assertIn('return 88',(await job(reader,'documents_read',{'path':'main.cpp'}))['text'])
                    await job(writer,'vcs_stash_save',{})
                    await job(writer,'vcs_stash_drop',{'index':0})
                    self.assertEqual((await job(reader,'vcs_inspect',{}))['stashes'],[])
                    self.assertIn('main.cpp',(await job(reader,'vcs_show',{'revision':'HEAD'}))['output'])
        asyncio.run(scenario())

    def test_msbuild_over_official_mcp(self):
        if not any(t['id']=='dotnet' and '8.0.' in t['version'] for t in self.client.call('msbuild.tools',{})['tools']):
            self.skipTest('The .NET 8 SDK is required for the managed fixture')
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        (self.workspace/'Managed.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework><OutputType>Exe</OutputType></PropertyGroup></Project>','utf8')
        (self.workspace/'Program.cs').write_text('System.Console.WriteLine("MCP_MSBUILD_42");\n','utf8')
        (self.workspace/'NuGet.Config').write_text('<configuration><packageSources><clear /></packageSources></configuration>','utf8')
        async def scenario():
            def parameters(name,permissions):
                return StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/name),'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
            async def job(session,name,inputs,expected='Completed'):
                response=await session.call_tool('op_'+name,{'parameters':inputs}); self.assertFalse(response.isError,response)
                identifier=json.loads(response.content[0].text)['jobId']; deadline=time.monotonic()+50
                while time.monotonic()<deadline:
                    response=await session.call_tool('cy_get_job',{'jobId':identifier}); result=json.loads(response.content[0].text)
                    if result['state'] in ('Completed','Failed','Cancelled'): break
                    await asyncio.sleep(.03)
                self.assertEqual(result['state'],expected,result)
                return result.get('outputs',{})
            async with stdio_client(parameters('msbuild-reader','')) as (ar,aw), stdio_client(parameters('msbuild-builder','process.execute')) as (br,bw):
                async with ClientSession(ar,aw) as reader,ClientSession(br,bw) as writer:
                    await reader.initialize(); await writer.initialize()
                    for operation in ('msbuild_build','msbuild_evaluate','msbuild_prepare','msbuild_clean'):
                        denied=await reader.call_tool('op_'+operation,{'parameters':{}}); self.assertTrue(denied.isError,denied)
                    inspected=await job(reader,'msbuild_inspect',{})
                    self.assertTrue(inspected['available']); self.assertEqual(inspected['profile']['entry'],'Managed.csproj')
                    self.assertFalse((self.workspace/'.cycode').exists(),'Inspection evaluated/saved project state')
                    await job(writer,'msbuild_restore',{'tool':'dotnet'})
                    built=await job(writer,'msbuild_build',{'tool':'dotnet'}); self.assertEqual(built['exitCode'],0,built)
                    evaluated=await job(writer,'msbuild_evaluate',{'tool':'dotnet'}); self.assertEqual(evaluated['exitCode'],0,evaluated)
                    artifact=evaluated['project']['artifacts'][0]['path']; self.assertTrue(Path(artifact).is_file(),artifact)
                    dotnet=next(t['executable'] for t in evaluated['project']['tools'] if t['id']=='dotnet')
                    self.assertIn('MCP_MSBUILD_42',subprocess.check_output([dotnet,artifact]).decode('utf8'))
                    await job(writer,'msbuild_inspect',{'entry':'../outside.csproj'},'Failed')
                    cleaned=await job(writer,'msbuild_clean',{}); self.assertEqual(cleaned['exitCode'],0,cleaned)
                    rebuilt=await job(writer,'msbuild_rebuild',{}); self.assertEqual(rebuilt['exitCode'],0,rebuilt)
                    self.assertTrue(Path(artifact).is_file())
        asyncio.run(scenario())

    def test_shared_core_service(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        descriptor = self.directory / 'bridge' / 'core.json'
        service = subprocess.Popen([sys.executable, str(APP / 'tool.py'), '--desktop-service',
            '--native-descriptor', str(self.descriptor), '--service-descriptor', str(descriptor),
            '--data-dir', str(self.directory / 'shared-jobs')], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while not descriptor.exists():
                if service.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Shared Core failed to start')
                time.sleep(.03)
            async def scenario():
                def parameters(access):
                    return StdioServerParameters(command=sys.executable,
                        args=[str(APP / 'tool.py'), '--bridge-config', str(descriptor), '--access', access, 'mcp'])
                async with stdio_client(parameters('read')) as (ar,aw), stdio_client(parameters('edit')) as (br,bw):
                    async with ClientSession(ar,aw) as reader, ClientSession(br,bw) as writer:
                        await reader.initialize()
                        await writer.initialize()
                        original = self.client.call('documents.read', {'path': 'main.cpp'})
                        edit = await writer.call_tool('op_documents_replace', {'parameters': {
                            'path':'main.cpp', 'text':'int main() { return 99; }\n',
                            'expectedRevision':original['revision']}})
                        self.assertFalse(edit.isError)
                        job = json.loads(edit.content[0].text)
                        hidden = await reader.call_tool('cy_get_job', {'jobId':job['jobId']})
                        self.assertTrue(hidden.isError)
                        for _ in range(100):
                            reply = await writer.call_tool('cy_get_job', {'jobId':job['jobId']})
                            done = json.loads(reply.content[0].text)
                            if done['state'] in ('Completed','Failed','Cancelled'):
                                break
                            await asyncio.sleep(.03)
                        self.assertEqual(done['state'], 'Completed', done)
                        self.assertIn('99', self.client.call('documents.read', {'path':'main.cpp'})['text'])
            asyncio.run(scenario())
        finally:
            self.descriptor.unlink(missing_ok=True)
            try:
                output, errors = service.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                service.kill()
                service.communicate()
                raise
            self.assertEqual(service.returncode, 0, errors.decode(errors='replace'))

    def test_semantic_highlighting_over_read_only_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        text='struct Widget { int Field; }; using Alias = Widget;\nint main() { const char* s = "🛠"; Alias Counter{}; return Counter.Field; }\n'
        (self.workspace/'main.cpp').write_text(text,'utf-8')
        (self.workspace/'Program.cs').write_text('class Counter { public int Value; }','utf-8')
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'colors-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':''})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def job(name,params,expected='Completed'):
                        response=await session.call_tool('op_'+name,{'parameters':params}); self.assertFalse(response.isError,response)
                        identifier=json.loads(response.content[0].text)['jobId']; deadline=time.monotonic()+25
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier}); done=json.loads(response.content[0].text)
                            if done['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(done['state'],expected,done); return done.get('outputs',{})
                            await asyncio.sleep(.03)
                        self.fail('Semantic color MCP timeout')
                    document=await job('documents_read',{'path':'main.cpp'})
                    colored=await job('language_highlight',{'path':'main.cpp','expectedRevision':document['revision']})
                    self.assertTrue(colored['supported']); self.assertFalse(colored['truncated'])
                    self.assertTrue(any(t['type'] in ('type','class','struct') for t in colored['tokens']))
                    self.assertTrue(any(t['type']=='property' for t in colored['tokens']))
                    self.assertTrue(any(t['character']>t['glyphStart'] for t in colored['tokens'] if t['line']==1))
                    status=await job('language_status',{})
                    self.assertTrue(status['servers']['cpp']['semanticTokens'])
                    edited=self.client.call('documents.replace',{'path':'main.cpp','expectedRevision':document['revision'],'text':text+'using OtherAlias = Widget;\n'})
                    await job('language_highlight',{'path':'main.cpp','expectedRevision':document['revision']},'Failed')
                    refreshed=await job('language_highlight',{'path':'main.cpp','expectedRevision':edited['revision']})
                    self.assertGreater(refreshed['generation'],colored['generation'])
                    csharp=await job('documents_read',{'path':'Program.cs'})
                    await job('language_highlight',{'path':'Program.cs','expectedRevision':csharp['revision']},'Failed')
                    self.assertFalse((await job('language_status',{}))['servers']['csharp']['enabled'])
                    invalid=await session.call_tool('op_language_highlight',{'parameters':{'path':'main.cpp'}})
                    self.assertTrue(invalid.isError)
                    self.assertEqual((self.workspace/'main.cpp').read_text('utf-8'),text)
        asyncio.run(scenario())

    def test_managed_debugger_over_official_mcp(self):
        from managed_debug_fixture import create_managed_debug_fixture
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        program,source,line = create_managed_debug_fixture(self.workspace)
        async def scenario():
            def parameters(name,permissions):
                return StdioServerParameters(command=sys.executable,
                    args=[str(APP/'tool.py'),'--data-dir',str(self.directory/name),'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
            async def job(session,name,inputs,expected='Completed'):
                response = await session.call_tool('op_'+name,{'parameters':inputs})
                self.assertFalse(response.isError,response)
                identifier = json.loads(response.content[0].text)['jobId']
                deadline = time.monotonic()+45
                while time.monotonic()<deadline:
                    response = await session.call_tool('cy_get_job',{'jobId':identifier})
                    result = json.loads(response.content[0].text)
                    if result['state'] in ('Completed','Failed','Cancelled'):
                        self.assertEqual(result['state'],expected,result)
                        return result.get('outputs',{})
                    await asyncio.sleep(.04)
                self.fail('Managed DAP MCP job timed out')
            async with stdio_client(parameters('managed-reader','')) as (ar,aw), stdio_client(parameters('managed-debugger','process.execute,workspace.write')) as (br,bw):
                async with ClientSession(ar,aw) as reader,ClientSession(br,bw) as writer:
                    await reader.initialize(); await writer.initialize()
                    denied = await reader.call_tool('op_debug_launch',{'parameters':{'runtime':'dotnet'}})
                    self.assertTrue(denied.isError,denied)
                    profile = await job(writer,'debug_configure',{'program':str(program),'runtime':'automatic','dotnet':'automatic','adapterArgs':[],
                        'args':['MCP é # "quoted"'],'env':{'CYCODE_DEBUG_MARKER':'MCP accent é'},'watches':['value + 1'],'buildBeforeLaunch':False})
                    self.assertEqual(profile['runtime'],'automatic')
                    await job(writer,'debug_breakpoints',{'path':'Program.cs','breakpoints':[{'line':line,'condition':'value == 41'}]})
                    try:
                        await job(writer,'debug_launch',{})
                        deadline = time.monotonic()+20
                        while time.monotonic()<deadline:
                            status = await job(writer,'debug_status',{})
                            if status['state']=='stopped' and status['variables'] and status.get('watches'): break
                            await asyncio.sleep(.05)
                        self.assertEqual(status['state'],'stopped',status)
                        self.assertEqual(status['runtime'],'dotnet')
                        self.assertEqual(status['frames'][0]['line'],line)
                        self.assertEqual(status['watches'][0]['result'],'42')
                        self.assertTrue(any(v['name']=='value' and v['value']=='41' for v in status['variables']))
                        self.assertTrue(any(v.get('verified') for v in status['verifiedBreakpoints']['Program.cs']))
                        self.assertEqual((await job(writer,'debug_evaluate',{'expression':'model.Value'}))['result'],'7')
                        reference = next(v['variablesReference'] for v in status['variables'] if v['name']=='values')
                        self.assertEqual(len((await job(writer,'debug_variables',{'variablesReference':reference,'start':1,'count':2}))['variables']),2)
                        await job(writer,'debug_continue',{})
                        deadline = time.monotonic()+20
                        while time.monotonic()<deadline:
                            status = await job(writer,'debug_status',{})
                            if status['state']=='exited' and 'RESULT=42' in status['output']: break
                            await asyncio.sleep(.05)
                        self.assertIn('RESULT=42 ARG=MCP é # "quoted" ENV=MCP accent é',status['output'])
                        self.assertEqual(status['frames'],[])
                        self.assertEqual(status['variables'],[])
                    finally:
                        await job(writer,'debug_stop',{})
                    apphost = program.with_suffix('.exe' if sys.platform=='win32' else '')
                    if apphost.is_file():
                        run = await job(writer,'debug_run',{'program':str(apphost)})
                        identifier = run['terminalId']
                        try:
                            deadline = time.monotonic()+20
                            while time.monotonic()<deadline:
                                screen = await job(writer,'terminal_read',{'id':identifier})
                                if any('RESULT=42 ARG=MCP é' in text for text in screen['lines']): break
                                await asyncio.sleep(.05)
                            self.assertTrue(any('RESULT=42 ARG=MCP é' in text for text in screen['lines']),screen)
                        finally:
                            await job(writer,'terminal_close',{'id':identifier})
                    self.assertEqual((self.workspace/'Program.cs').read_text('utf-8'),source)
        asyncio.run(scenario())

    def test_csharp_official_mcp_and_permissions(self):
        from csharp_fixture import create_csharp_fixture, CONSUMER
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        create_csharp_fixture(self.workspace)
        self.assertFalse(self.client.call('language.status', {})['servers']['csharp']['enabled'])
        runtime = create_runtime(self.directory/'csharp-permissions')
        runtime.start()
        try:
            reader = runtime.authenticate(runtime.create_client('C# reader')['token'])
            reader_session = runtime.create_session(reader, temporary=True)['sessionId']
            with self.assertRaises(Exception):
                runtime.submit(reader, reader_session, 'language_configure', {})
        finally:
            runtime.close()
        self.assertFalse(self.client.call('language.status', {})['servers']['csharp']['enabled'])
        async def scenario():
            parameters = StdioServerParameters(command=sys.executable,
                args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'csharp-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':'process.execute,workspace.write'})
            async with stdio_client(parameters) as (read, write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def invoke(name,parameters):
                        response = await session.call_tool('op_'+name,{'parameters':parameters})
                        self.assertFalse(response.isError,response)
                        job = json.loads(response.content[0].text)
                        deadline = time.monotonic()+75
                        while time.monotonic()<deadline:
                            response = await session.call_tool('cy_get_job',{'jobId':job['jobId']})
                            done = json.loads(response.content[0].text)
                            if done['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(done['state'],'Completed',done)
                                return done['outputs']
                            await asyncio.sleep(.08)
                        self.fail('C# MCP job did not complete')
                    await invoke('language_configure',{})
                    doc = await invoke('documents_read',{'path':'App/Consumer.cs'})
                    self.assertEqual(doc['text'],CONSUMER)
                    position = {'line':1,'character':CONSUMER.splitlines()[1].index('Value')}
                    request = {'path':'App/Consumer.cs','expectedRevision':doc['revision'],'position':position}
                    definition = await invoke('language_definition',request)
                    self.assertEqual(definition['locations'][0]['path'],'Library/Counter.cs')
                    references = await invoke('language_references',request)
                    self.assertEqual(len(references['locations']),2)
                    counter = await invoke('documents_read',{'path':'Library/Counter.cs'})
                    await invoke('documents_replace',{'path':'Library/Counter.cs','expectedRevision':counter['revision'],
                        'text':'namespace Demo; public class Counter { public int Value; public int OnlyInBuffer; }\n'})
                    text = CONSUMER.replace('c.Value','c.On')
                    doc = await invoke('documents_replace',{'path':'App/Consumer.cs','expectedRevision':doc['revision'],'text':text})
                    completion = await invoke('language_complete',{'path':'App/Consumer.cs','expectedRevision':doc['revision'],
                        'position':{'line':1,'character':text.splitlines()[1].index('c.On')+4}})
                    self.assertIn('OnlyInBuffer',json.dumps(completion))
                    status = await invoke('language_status',{})
                    self.assertTrue(status['servers']['csharp']['ready'])
                    await invoke('language_configure',{'enabled':False})
                    self.assertNotIn('OnlyInBuffer',(self.workspace/'Library/Counter.cs').read_text('utf-8'))
        asyncio.run(scenario())

if __name__ == '__main__':
    unittest.main()

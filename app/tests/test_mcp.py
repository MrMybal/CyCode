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
    def test_events_over_read_only_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        (self.workspace/'events.cpp').write_text('int value=0;\n','utf-8')
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,
                args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'events-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':''})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def invoke(inputs, operation='events_read'):
                        response=await session.call_tool('op_'+operation,{'parameters':inputs})
                        self.assertFalse(response.isError,response)
                        identifier=json.loads(response.content[0].text)['jobId']
                        deadline=time.monotonic()+10
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier})
                            job=json.loads(response.content[0].text)
                            if job['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(job['state'],'Completed',job)
                                return job['outputs']
                            await asyncio.sleep(.03)
                        self.fail('Event metadata read did not complete')
                    inventory=await invoke({},'plugins_list')
                    self.assertIn('workspaceEvents',inventory['host']['features'])
                    self.assertIn('eventSubscriptions',inventory)
                    self.assertIn('debugAdapters',inventory['host']['features'])
                    self.assertIn('debugAdapters',inventory)
                    adapters=await invoke({},'debug_adapters')
                    self.assertEqual(adapters['adapters'][0]['id'],'builtin')
                    self.assertTrue(all('arguments' not in entry and 'environment' not in entry for entry in adapters['adapters']))
                    for operation in ('debug_configure','debug_launch','debug_status'):
                        denied=await session.call_tool('op_'+operation,{'parameters':{}})
                        self.assertTrue(denied.isError,denied)
                    start=await invoke({'tail':True})
                    doc=self.client.call('documents.read',{'path':'events.cpp'})
                    self.client.call('documents.replace',{'path':'events.cpp','expectedRevision':doc['revision'],'text':'int value=1;\n'})
                    batch=await invoke({'epoch':start['epoch'],'afterSequence':start['nextSequence'],'types':['document.*']})
                    self.assertFalse(batch['gap'],batch)
                    self.assertEqual([event['type'] for event in batch['events']],['document.opened','document.changed'])
                    self.assertTrue(all('text' not in event['data'] for event in batch['events']))
                    empty=await invoke({'epoch':batch['epoch'],'afterSequence':batch['nextSequence']})
                    self.assertEqual(empty['events'],[])
        asyncio.run(scenario())

    def test_build_cancellation_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        (self.workspace/'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.24)\nproject(CancelBuild NONE)\n'
            'if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/allow-fast")\n'
            ' file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/entered-configure" "ready")\n'
            ' execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 30)\nendif()\n', 'utf-8')
        runtime=create_runtime(self.directory/'build-cancel-permissions');runtime.start()
        try:
            reader=runtime.authenticate(runtime.create_client('reader')['token'])
            session_id=runtime.create_session(reader,temporary=True)['sessionId']
            with self.assertRaises(Exception):runtime.submit(reader,session_id,'build_cancel',{})
            status=runtime.submit(reader,session_id,'build_status',{})
            self.assertEqual(runtime.wait(reader,status['jobId'],timeout=10)['state'],'Completed')
        finally:runtime.close()
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'build-cancel-mcp'),'mcp'],env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':'process.execute'})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def submit(name,inputs):
                        response=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(response.isError,response)
                        return json.loads(response.content[0].text)['jobId']
                    async def done(identifier,expected='Completed'):
                        deadline=time.monotonic()+25
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                            if job['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(job['state'],expected,job);return job.get('outputs',{})
                            await asyncio.sleep(.04)
                        self.fail('Native build MCP job did not stop')
                    running=await submit('build_configure',{'buildDirectory':'build-cancel'})
                    deadline=time.monotonic()+15
                    while not (self.workspace/'entered-configure').exists() and time.monotonic()<deadline:
                        await asyncio.sleep(.04)
                    self.assertTrue((self.workspace/'entered-configure').exists(),'CMake did not enter its native execution phase')
                    active=self.client.call('build.status',{});self.assertTrue(active['running'])
                    stale=await done(await submit('build_cancel',{'operationId':'stale-operation'}));self.assertFalse(stale['requested'])
                    self.assertTrue(self.client.call('build.status',{})['running'])
                    response=await session.call_tool('cy_cancel_job',{'jobId':running});self.assertFalse(response.isError,response)
                    await done(running,'Cancelled')
                    self.assertFalse(self.client.call('build.status',{})['running'])
                    (self.workspace/'allow-fast').write_text('ok','utf-8')
                    result=await done(await submit('build_configure',{'buildDirectory':'build-next'}))
                    self.assertEqual(result['exitCode'],0,result);self.assertFalse(result['cancelled'])
        asyncio.run(scenario())

    def test_ctest_and_hunks_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        cmake=self.client.call('toolchains.detect',{})['cmake']
        self.assertTrue(cmake)
        (self.workspace/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(McpTests NONE)\nenable_testing()\nadd_test(NAME passing COMMAND "${CMAKE_COMMAND}" -E true)\nadd_test(NAME delayed COMMAND "${CMAKE_COMMAND}" -E sleep 10)\n','utf-8')
        subprocess.run([cmake,'-S',str(self.workspace),'-B',str(self.workspace/'build')],check=True,capture_output=True)
        def git(*args):
            return subprocess.check_output(['git','-C',str(self.workspace),*args],stderr=subprocess.STDOUT)
        git('init','--initial-branch=main');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
        git('add','main.cpp');git('commit','-m','Initial')
        (self.workspace/'main.cpp').write_text('int main() { return 42; }\n','utf-8')
        runtime=create_runtime(self.directory/'workflow-permissions');runtime.start()
        try:
            reader=runtime.authenticate(runtime.create_client('reader')['token'])
            session=runtime.create_session(reader,temporary=True)['sessionId']
            for operation,parameters in [('tests_discover',{}),('tests_run',{'id':'none','tests':[0]}),('tests_cancel',{}),('vcs_apply_hunks',{'id':'none','hunks':[0]})]:
                with self.assertRaises(Exception):runtime.submit(reader,session,operation,parameters)
            job=runtime.submit(reader,session,'vcs_hunks',{'path':'main.cpp'})
            self.assertEqual(runtime.wait(reader,job['jobId'],timeout=15)['state'],'Completed')
        finally:runtime.close()
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'workflow-mcp'),'mcp'],env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':'process.execute,vcs.write'})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def submit(name,inputs):
                        response=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(response.isError,response)
                        return json.loads(response.content[0].text)['jobId']
                    async def done(identifier,expected='Completed'):
                        deadline=time.monotonic()+30
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                            if job['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(job['state'],expected,job);return job.get('outputs',{})
                            await asyncio.sleep(.04)
                        self.fail('Workflow MCP job did not stop')
                    catalogue=await done(await submit('tests_discover',{}));self.assertEqual(len(catalogue['tests']),2)
                    tested=await done(await submit('tests_run',{'id':catalogue['id'],'tests':[0]}));self.assertEqual(tested['results'][0]['status'],'passed')
                    running=await submit('tests_run',{'id':catalogue['id'],'tests':[1]})
                    await asyncio.sleep(.3)
                    response=await session.call_tool('cy_cancel_job',{'jobId':running});self.assertFalse(response.isError,response)
                    await done(running,'Cancelled')
                    tested=await done(await submit('tests_run',{'id':catalogue['id'],'tests':[0]}));self.assertEqual(tested['exitCode'],0)
                    hunks=await done(await submit('vcs_hunks',{'path':'main.cpp'}));self.assertEqual(len(hunks['hunks']),1)
                    applied=await done(await submit('vcs_apply_hunks',{'id':hunks['id'],'hunks':[0]}));self.assertEqual(applied['exitCode'],0)
                    self.assertIn(b'return 42',git('show',':main.cpp'))
        asyncio.run(scenario())

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

    def test_project_templates_and_permissions(self):
        runtime = create_runtime(self.directory / 'template-jobs')
        runtime.start()
        try:
            reader = runtime.authenticate(runtime.create_client('reader')['token'])
            writer = runtime.authenticate(runtime.create_client('writer', ['workspace.write'])['token'])
            sessions = {name: runtime.create_session(actor, temporary=True)['sessionId'] for name, actor in [('reader', reader), ('writer', writer)]}
            def invoke(actor, name, operation, parameters, state='Completed'):
                job = runtime.submit(actor, sessions[name], operation, parameters)
                done = runtime.wait(actor, job['jobId'], timeout=10)
                self.assertEqual(done['state'], state, done)
                return done.get('outputs')
            catalogue = invoke(reader, 'reader', 'templates_list', {})
            self.assertTrue({'cpp.console','c.console','rust.console','csharp.console','java.console','web.javascript','cycode.unreal.cppGame'} <= {item['id'] for item in catalogue['templates']})
            plan = invoke(reader, 'reader', 'templates_preview', {'template': 'csharp.console', 'name': 'HelloMcp'})
            self.assertFalse((self.workspace / 'HelloMcp').exists())
            with self.assertRaises(Exception):
                runtime.submit(reader, sessions['reader'], 'templates_create', {'id': plan['id']})
            result = invoke(writer, 'writer', 'templates_create', {'id': plan['id']})
            self.assertEqual(result['state'], 'created')
            self.assertEqual(result['entry'], 'HelloMcp/HelloMcp.sln')
            for item in plan['files']:
                self.assertEqual((self.workspace / 'HelloMcp' / item['path']).read_bytes(), item['text'].encode('utf-8'))
            invoke(writer, 'writer', 'templates_create', {'id': plan['id']}, 'Failed')
            invoke(reader, 'reader', 'templates_preview', {'template': 'cpp.console', 'name': 'Safe', 'destination': '../Outside'}, 'Failed')
            collision = invoke(reader, 'reader', 'templates_preview', {'template': 'rust.console', 'name': 'Collision'})
            (self.workspace / 'Collision').mkdir()
            (self.workspace / 'Collision/keep').write_text('external', encoding='utf-8')
            invoke(writer, 'writer', 'templates_create', {'id': collision['id']}, 'Failed')
            self.assertEqual((self.workspace / 'Collision/keep').read_text('utf-8'), 'external')
            unreal = invoke(reader, 'reader', 'templates_preview', {'template': 'cycode.unreal.cppGame', 'name': 'McpGame', 'options': {'engineAssociation': '5.3'}})
            self.assertEqual(unreal['plugin'], 'cycode.unreal')
            self.assertEqual(unreal['options']['engineAssociation'], '5.3')
            with self.assertRaises(Exception):
                runtime.submit(reader, sessions['reader'], 'templates_create', {'id': unreal['id']})
            created = invoke(writer, 'writer', 'templates_create', {'id': unreal['id']})
            self.assertEqual(created['entry'], 'McpGame/McpGame.uproject')
            self.assertEqual(json.loads((self.workspace / 'McpGame/McpGame.uproject').read_text('utf-8'))['Modules'][0]['Name'], 'McpGame')
            invoke(reader, 'reader', 'templates_preview', {'template': 'cycode.unreal.cppGame', 'name': 'Invalid', 'options': {'engineAssociation': '5.3"bad'}}, 'Failed')
            stale = invoke(reader, 'reader', 'templates_preview', {'template': 'cycode.unreal.cppGame', 'name': 'StaleGame'})
            admin = runtime.authenticate(runtime.create_client('admin', ['plugins.execute', 'workspace.write', 'process.execute'])['token'])
            sessions['admin'] = runtime.create_session(admin, temporary=True)['sessionId']
            inventory = invoke(reader, 'reader', 'plugins_list', {})
            self.assertTrue(any(item['id'] == 'cycode.unreal.cppGame' for item in inventory['templates']))
            disabled = invoke(admin, 'admin', 'plugins_configure', {'id': 'cycode.unreal', 'enabled': False, 'expectedRevision': inventory['revision']})
            self.assertFalse(any(item['id'] == 'cycode.unreal.cppGame' for item in invoke(reader, 'reader', 'templates_list', {})['templates']))
            invoke(writer, 'writer', 'templates_create', {'id': stale['id']}, 'Failed')
            invoke(admin, 'admin', 'plugins_configure', {'id': 'cycode.unreal', 'enabled': True, 'expectedRevision': disabled['revision']})
            invoke(writer, 'writer', 'templates_create', {'id': stale['id']}, 'Failed')
            self.assertFalse((self.workspace / 'StaleGame').exists())
        finally:
            runtime.close()
        self.assertTrue((self.workspace / 'HelloMcp/HelloMcp.sln').exists())
        self.assertFalse((self.workspace / 'HelloMcp/LICENSE').exists())

    def test_resource_transactions_and_permissions(self):
        runtime = create_runtime(self.directory / 'resource-jobs')
        runtime.start()
        try:
            reader = runtime.authenticate(runtime.create_client('reader')['token'])
            writer = runtime.authenticate(runtime.create_client('writer', ['workspace.write'])['token'])
            sessions = {name: runtime.create_session(actor, temporary=True)['sessionId'] for name, actor in [('reader', reader), ('writer', writer)]}
            def invoke(actor, name, operation, parameters, state='Completed'):
                job = runtime.submit(actor, sessions[name], operation, parameters)
                done = runtime.wait(actor, job['jobId'], timeout=10)
                self.assertEqual(done['state'], state, done)
                return done.get('outputs')
            doc = invoke(reader, 'reader', 'documents_read', {'path': 'main.cpp'})
            plan = invoke(reader, 'reader', 'resources_preview', {'operation': 'move', 'path': 'main.cpp', 'destination': 'renommé.cpp'})
            self.assertTrue((self.workspace / 'main.cpp').exists())
            with self.assertRaises(Exception):
                runtime.submit(reader, sessions['reader'], 'resources_apply', {'id': plan['id']})
            edited = invoke(writer, 'writer', 'documents_replace', {'path': 'main.cpp', 'text': '// modified\n', 'expectedRevision': doc['revision']})
            invoke(writer, 'writer', 'resources_apply', {'id': plan['id']}, 'Failed')
            invoke(writer, 'writer', 'documents_save', {'path': 'main.cpp', 'expectedRevision': edited['revision']})
            plan = invoke(reader, 'reader', 'resources_preview', {'operation': 'move', 'path': 'main.cpp', 'destination': 'renommé.cpp'})
            moved = invoke(writer, 'writer', 'resources_apply', {'id': plan['id']})
            renamed = invoke(reader, 'reader', 'documents_read', {'path': 'renommé.cpp'})
            self.assertEqual(renamed['id'], doc['id'])
            self.assertFalse((self.workspace / 'main.cpp').exists())
            children = invoke(reader, 'reader', 'resources_children', {'path': '.'})
            self.assertEqual([item['path'] for item in children['items']], ['renommé.cpp'])
            events = invoke(reader, 'reader', 'resources_events', {})
            self.assertEqual(events['events'][-1]['to'], 'renommé.cpp')
            deletion = invoke(reader, 'reader', 'resources_preview', {'operation': 'delete', 'path': 'renommé.cpp'})
            removed = invoke(writer, 'writer', 'resources_apply', {'id': deletion['id']})
            trash = invoke(reader, 'reader', 'resources_trash', {})
            self.assertEqual(trash['items'][0]['id'], removed['trash'])
            restore = invoke(reader, 'reader', 'resources_preview', {'operation': 'restore', 'trash': removed['trash']})
            self.assertFalse(restore['blocked'])
            invoke(writer, 'writer', 'resources_apply', {'id': restore['id']})
            with self.assertRaises(Exception):
                runtime.submit(reader, sessions['reader'], 'resources_undo', {'id': moved['id']})
            invoke(writer, 'writer', 'resources_undo', {'id': moved['id']})
            self.assertEqual((self.workspace / 'main.cpp').read_text('utf-8'), '// modified\n')
            invoke(reader, 'reader', 'resources_preview', {'operation': 'delete', 'path': '../outside'}, 'Failed')
            self.assertTrue((self.workspace / 'main.cpp').exists())
        finally:
            runtime.close()

    def test_recovery_native_runtime_permissions_and_restart(self):
        runtime = create_runtime(self.directory / 'recovery-jobs')
        runtime.start()
        try:
            reader = runtime.authenticate(runtime.create_client('reader')['token'])
            writer = runtime.authenticate(runtime.create_client('writer', ['workspace.write'])['token'])
            sessions = {name: runtime.create_session(actor, temporary=True)['sessionId'] for name, actor in [('reader', reader), ('writer', writer)]}
            def invoke(actor, name, operation, parameters, state='Completed'):
                job = runtime.submit(actor, sessions[name], operation, parameters)
                done = runtime.wait(actor, job['jobId'], timeout=10)
                self.assertEqual(done['state'], state, done)
                return done.get('outputs')
            before = (self.workspace / 'main.cpp').read_bytes()
            doc = self.client.call('documents.read', {'path': 'main.cpp'})
            edited = '// récupéré 🛠\nint main() { return 73; }\n'
            invoke(writer, 'writer', 'documents_replace', {'path': 'main.cpp', 'text': edited, 'expectedRevision': doc['revision']})
            status = invoke(writer, 'writer', 'recovery_flush', {})
            self.assertEqual(status['documents'], 1)
            self.assertFalse(status['errors'])
            inventory = invoke(reader, 'reader', 'recovery_list', {})
            entry = inventory['items'][0]
            self.assertTrue(entry['active'])
            ref = {key: entry[key] for key in ('session', 'id', 'checkpoint')}
            invoke(reader, 'reader', 'recovery_preview', ref, 'Failed')
            for operation, parameters in [('recovery_flush', {}), ('recovery_discard', ref), ('recovery_restore', {**ref, 'expectedRevision': 0, 'expectedDiskState': 'missing'})]:
                with self.assertRaises(Exception):
                    runtime.submit(reader, sessions['reader'], operation, parameters)
            # Terminate the native process, bypassing its destructor and leaving the real checkpoint.
            self.process.kill()
            self.process.communicate(timeout=10)
            self.descriptor.unlink(missing_ok=True)
            executable = Path(os.environ['CYCODE_TEST_CLI']).resolve()
            self.process = subprocess.Popen([str(executable), str(self.workspace), '--serve', str(self.descriptor)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            deadline = time.monotonic() + 10
            while not self.descriptor.exists():
                if self.process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Native bridge did not restart')
                time.sleep(.02)
            preview = invoke(reader, 'reader', 'recovery_preview', ref)
            self.assertEqual(preview['text'], edited)
            self.assertEqual(preview['targetRevision'], 0)
            parameters = {**ref, 'expectedRevision': preview['targetRevision'], 'expectedDiskState': preview['diskState']}
            invoke(writer, 'writer', 'recovery_restore', {**parameters, 'expectedRevision': 999}, 'Failed')
            recovered = invoke(writer, 'writer', 'recovery_restore', parameters)
            self.assertTrue(recovered['checkpointProtected'])
            self.assertFalse(recovered['sourceRetained'])
            self.assertEqual(recovered['document']['text'], edited)
            self.assertEqual((self.workspace / 'main.cpp').read_bytes(), before)
            invoke(writer, 'writer', 'documents_save', {'path': 'main.cpp', 'expectedRevision': recovered['document']['revision']})
            self.assertEqual(invoke(reader, 'reader', 'recovery_list', {})['items'], [])
        finally:
            runtime.close()

    def test_extensions_provider_contracts_and_permissions(self):
        runtime = create_runtime(self.directory / 'extension-jobs')
        runtime.start()
        try:
            reader = runtime.authenticate(runtime.create_client('reader')['token'])
            partial = runtime.authenticate(runtime.create_client('plugin-only', ['plugins.execute'])['token'])
            editor = runtime.authenticate(runtime.create_client('editor', ['plugins.execute', 'workspace.write', 'process.execute', 'vcs.write'])['token'])
            sessions = {name: runtime.create_session(actor, temporary=True)['sessionId'] for name, actor in [('reader', reader), ('partial', partial), ('editor', editor)]}
            def invoke(actor, name, operation, parameters, state='Completed'):
                job = runtime.submit(actor, sessions[name], operation, parameters)
                done = runtime.wait(actor, job['jobId'], timeout=10)
                self.assertEqual(done['state'], state, done)
                return done.get('outputs')
            inventory = invoke(reader, 'reader', 'plugins_list', {})
            self.assertEqual(inventory['supportedAbis'], [1, 2])
            self.assertTrue(any(p['id'] == 'cycode.unreal' for p in inventory['plugins']))
            for actor, name in [(reader, 'reader'), (partial, 'partial')]:
                with self.assertRaises(Exception):
                    runtime.submit(actor, sessions[name], 'plugins_invoke', {'command': 'sample.text_statistics', 'input': {'text': 'hello'}})
            sample_id = 'cycode.sample'
            disabled = invoke(editor, 'editor', 'plugins_configure', {'id': sample_id, 'enabled': False, 'expectedRevision': inventory['revision']})
            invoke(editor, 'editor', 'plugins_invoke', {'command': 'sample.text_statistics', 'input': {'text': 'hello'}}, 'Failed')
            enabled = invoke(editor, 'editor', 'plugins_configure', {'id': sample_id, 'enabled': True, 'expectedRevision': disabled['revision']})
            self.assertGreater(enabled['revision'], disabled['revision'])
            self.assertEqual(invoke(editor, 'editor', 'plugins_invoke', {'command': 'sample.text_statistics', 'input': {'text': 'hello world'}})['words'], 2)
            providers = invoke(reader, 'reader', 'vcs_providers', {})
            self.assertEqual(providers['providers'][0]['id'], 'git')
            self.assertTrue(any(p['id']=='cycode.perforce.provider' for p in providers['providers']))
            self.assertTrue(any(p['id']=='cycode.svn.provider' for p in providers['providers']))
            self.assertTrue(any(p['id']=='cycode.mercurial.provider' for p in providers['providers']))
            self.assertTrue(any(p['id']=='cycode.diversion.provider' for p in providers['providers']))
            context = invoke(reader, 'reader', 'vcs_context', {'contextPath': 'main.cpp'})
            mapped = invoke(editor, 'editor', 'vcs_configure', {'root':'.','provider':'cycode.perforce.provider','expectedRevision':context['revision']})
            self.assertEqual(invoke(reader, 'reader', 'vcs_context', {})['provider'], 'cycode.perforce.provider')
            self.assertEqual(invoke(reader, 'reader', 'vcs_status', {})['provider'], 'git')
            for provider in ('cycode.perforce.provider','cycode.svn.provider','cycode.mercurial.provider','cycode.diversion.provider'):
                with self.assertRaises(Exception):
                    runtime.submit(reader, sessions['reader'], 'vcs_status', {'provider':provider})
                with self.assertRaises(Exception):
                    runtime.submit(reader, sessions['reader'], 'vcs_provider_action', {'provider':provider,'action':'status'})
            context = invoke(reader, 'reader', 'vcs_context', {})
            with self.assertRaises(Exception):
                runtime.submit(reader, sessions['reader'], 'vcs_configure', {'root': '.', 'provider': 'git', 'expectedRevision': context['revision']})
            invoke(editor, 'editor', 'vcs_configure', {'root': '.', 'provider': 'git', 'expectedRevision': context['revision']})
            for filename in ['a.txt', 'b.txt']:
                (self.workspace / filename).write_text(filename, encoding='utf-8')
            page = invoke(reader, 'reader', 'workspace_files', {'limit': 1, 'refresh': True})
            paths = page['files'][:]
            while page['nextOffset'] is not None:
                page = invoke(reader, 'reader', 'workspace_files', {'limit': 1, 'offset': page['nextOffset']})
                paths += page['files']
            self.assertEqual(set(paths), {'a.txt', 'b.txt', 'main.cpp'})
            self.assertEqual(page['total'], 3)
            poll = invoke(reader, 'reader', 'workspace_poll', {})
            self.assertIn(poll['watcher']['backend'], ['ReadDirectoryChangesW', 'polling'])
        finally:
            runtime.close()

    def test_extension_registry_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        async def scenario():
            parameters = StdioServerParameters(command=sys.executable, args=[str(APP / 'tool.py'), '--data-dir', str(self.directory / 'extensions-mcp'), 'mcp'], env={**os.environ, 'CYCODE_BRIDGE': str(self.descriptor), 'CYCODE_PERMISSIONS': ''})
            async with stdio_client(parameters) as (read, write):
                async with ClientSession(read, write) as session:
                    await session.initialize()
                    tools = await session.list_tools()
                    names = {tool.name for tool in tools.tools}
                    self.assertTrue({'op_resources_preview', 'op_resources_apply', 'op_resources_undo', 'op_resources_children', 'op_resources_trash', 'op_resources_events'} <= names)
                    self.assertTrue({'op_templates_list', 'op_templates_preview', 'op_templates_create'} <= names)
                    self.assertTrue({'op_plugins_configure', 'op_plugins_project', 'op_vcs_context', 'op_vcs_providers', 'op_workspace_poll', 'op_recovery_list', 'op_recovery_preview', 'op_recovery_restore', 'op_recovery_discard', 'op_recovery_flush', 'op_recovery_status'} <= names)
                    response = await session.call_tool('op_plugins_list', {'parameters': {}})
                    self.assertFalse(response.isError, response)
                    identifier = json.loads(response.content[0].text)['jobId']
                    deadline = time.monotonic() + 10
                    while True:
                        result = await session.call_tool('cy_get_job', {'jobId': identifier})
                        job = json.loads(result.content[0].text)
                        if job['state'] in ('Completed', 'Failed', 'Cancelled'):
                            self.assertEqual(job['state'], 'Completed', job)
                            self.assertEqual(job['outputs']['abi'], 2)
                            break
                        self.assertLess(time.monotonic(), deadline)
                        await asyncio.sleep(.03)
                    denied = await session.call_tool('op_plugins_invoke', {'parameters': {'command': 'sample.text_statistics', 'input': {'text': 'hello'}}})
                    self.assertTrue(denied.isError)
        asyncio.run(scenario())

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
                    for operation in ('debug_profile','debug_configure','debug_status','debug_breakpoints','debug_launch','debug_attach','debug_run','debug_continue','debug_pause','debug_next','debug_step_in','debug_step_out','debug_stop','debug_evaluate','debug_variables','debug_source','debug_memory','debug_disassemble','debug_exception_info','debug_function_breakpoints','debug_exception_breakpoints'):
                        self.assertIn('op_'+operation,tools)
                    denied=await reader.call_tool('op_debug_status',{'parameters':{}})
                    self.assertTrue(denied.isError,denied)
                    for name,inputs in (('debug_memory',{'memoryReference':'x'}),('debug_disassemble',{'memoryReference':'x'}),('debug_exception_info',{}),('debug_function_breakpoints',{'breakpoints':[]}),('debug_exception_breakpoints',{'filters':None})):
                        refused=await reader.call_tool('op_'+name,{'parameters':inputs})
                        self.assertTrue(refused.isError,refused)
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

    def test_web_official_mcp_and_permissions(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        text="const message = 'hello';\nmessage.toU\n"
        (self.workspace/'main.ts').write_text(text,encoding='utf-8')
        status=self.client.call('language.status',{})
        if not status['servers']['typescript']['available']:
            self.skipTest('Install web language servers with setup_languages.py --web')
        self.assertFalse(status['servers']['typescript']['enabled'])
        runtime=create_runtime(self.directory/'web-permissions');runtime.start()
        try:
            reader=runtime.authenticate(runtime.create_client('web reader')['token'])
            session=runtime.create_session(reader,temporary=True)['sessionId']
            with self.assertRaises(Exception):
                runtime.submit(reader,session,'language_configure',{'language':'typescript'})
            with self.assertRaises(Exception):
                runtime.submit(reader,session,'language_preferences_forget',{'language':'typescript','expectedPreferencesRevision':0})
            preferences=runtime.submit(reader,session,'language_preferences',{})
            self.assertEqual(runtime.wait(reader,preferences['jobId'],timeout=10)['state'],'Completed')
        finally:
            runtime.close()
        self.assertFalse(self.client.call('language.status',{})['servers']['typescript']['enabled'])
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,
                args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'web-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':'process.execute,workspace.write'})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def invoke(name,parameters):
                        response=await session.call_tool('op_'+name,{'parameters':parameters})
                        self.assertFalse(response.isError,response)
                        job=json.loads(response.content[0].text)
                        deadline=time.monotonic()+40
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':job['jobId']})
                            done=json.loads(response.content[0].text)
                            if done['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(done['state'],'Completed',done)
                                return done['outputs']
                            await asyncio.sleep(.06)
                        self.fail('Web MCP job did not complete')
                    doc=await invoke('documents_read',{'path':'main.ts'})
                    preferences=await invoke('language_preferences',{})
                    self.assertTrue(preferences['available'],preferences)
                    saved=await invoke('language_configure',{'language':'typescript','enabled':False,
                        'arguments':[],'settings':{},'save':True,'expectedPreferencesRevision':preferences['revision']})
                    self.assertFalse(saved['servers']['typescript']['enabled'])
                    preferences=await invoke('language_preferences',{})
                    self.assertEqual(preferences['profiles']['typescript']['arguments'],[])
                    forgotten=await invoke('language_preferences_forget',{'language':'typescript','expectedPreferencesRevision':preferences['revision']})
                    self.assertNotIn('typescript',forgotten['profiles'])
                    await invoke('language_configure',{'language':'typescript'})
                    completion=await invoke('language_complete',{'path':'main.ts','expectedRevision':doc['revision'],
                        'position':{'line':1,'character':11}})
                    self.assertIn('toUpperCase',json.dumps(completion))
                    updated=text.replace("'hello'","42").replace('toU','toF')
                    doc=await invoke('documents_replace',{'path':'main.ts','expectedRevision':doc['revision'],'text':updated})
                    completion=await invoke('language_complete',{'path':'main.ts','expectedRevision':doc['revision'],
                        'position':{'line':1,'character':11}})
                    self.assertIn('toFixed',json.dumps(completion))
                    status=await invoke('language_status',{})
                    self.assertTrue(status['servers']['typescript']['ready'])
                    self.assertTrue(status['servers']['typescript']['methods']['completion'])
                    self.assertTrue({'cpp','csharp','rust','java','typescript','html','css','python','hlsl','glsl'} <= set(status['servers']))
                    await invoke('language_configure',{'language':'typescript','enabled':False})
        asyncio.run(scenario())
        self.assertEqual((self.workspace/'main.ts').read_text('utf-8'),text)

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

    def test_project_tasks_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        (self.workspace/'package.json').write_text(json.dumps({'name':'mcp-tasks','private':True,'scripts':{
            'build':'node build.cjs','test':'node test.cjs','start':'node app.cjs','long':'node long.cjs'}}),'utf-8')
        (self.workspace/'build.cjs').write_text("require('node:fs').writeFileSync('built','yes'); console.log('BUILD_OK');",'utf-8')
        (self.workspace/'test.cjs').write_text("console.log('TEST_OK');",'utf-8')
        (self.workspace/'app.cjs').write_text("console.log('RUN_OK '+process.argv[2]);",'utf-8')
        (self.workspace/'long.cjs').write_text("require('node:fs').writeFileSync('started','yes'); require('node:child_process').spawn(process.execPath,['-e',\"setTimeout(()=>require('node:fs').writeFileSync('survived','bad'),4000)\"],{stdio:'ignore'}); setInterval(()=>{},1000);",'utf-8')
        model=self.client.call('project.inspect',{})
        if not model['tools']['npm']:
            self.skipTest('Node.js/npm unavailable')
        self.assertEqual(model['kind'],'npm')
        self.assertFalse((self.workspace/'built').exists())
        runtime=create_runtime(self.directory/'project-permissions');runtime.start()
        try:
            actor=runtime.authenticate(runtime.create_client('reader')['token'])
            session=runtime.create_session(actor,temporary=True)['sessionId']
            inspected=runtime.submit(actor,session,'project_inspect',{})
            self.assertEqual(runtime.wait(actor,inspected['jobId'],timeout=10)['state'],'Completed')
            for operation in ('configure','build','test','install','script','launch','debug_target','cancel'):
                with self.assertRaises(Exception,msg=operation): runtime.submit(actor,session,'project_'+operation,{})
        finally:
            runtime.close()
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,
                args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'project-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':'process.execute'})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def submit(name,inputs):
                        response=await session.call_tool('op_'+name,{'parameters':inputs})
                        self.assertFalse(response.isError,response)
                        return json.loads(response.content[0].text)['jobId']
                    async def done(identifier,expected='Completed'):
                        deadline=time.monotonic()+20
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier})
                            job=json.loads(response.content[0].text)
                            if job['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(job['state'],expected,job);return job.get('outputs',{})
                            await asyncio.sleep(.04)
                        self.fail('Project MCP task did not stop')
                    built=await done(await submit('project_build',{}));self.assertIn('BUILD_OK',built['output'])
                    tested=await done(await submit('project_test',{}));self.assertIn('TEST_OK',tested['output'])
                    launched=await done(await submit('project_launch',{'args':['a & b é']}))
                    terminal=launched['terminal']['id']
                    deadline=time.monotonic()+8
                    while time.monotonic()<deadline:
                        output=await done(await submit('terminal_read',{'id':terminal}))
                        if 'RUN_OK a & b é' in '\n'.join(output['lines']): break
                        await asyncio.sleep(.04)
                    self.assertIn('RUN_OK a & b é','\n'.join(output['lines']))
                    await done(await submit('terminal_close',{'id':terminal}))
                    for manual in (False,True):
                        (self.workspace/'started').unlink(missing_ok=True)
                        identifier=await submit('project_script',{'script':'long'})
                        deadline=time.monotonic()+10
                        while not (self.workspace/'started').exists() and time.monotonic()<deadline: await asyncio.sleep(.03)
                        self.assertTrue((self.workspace/'started').exists())
                        if manual:
                            cancelled=await done(await submit('project_cancel',{}));self.assertTrue(cancelled['cancelled'])
                            result=await done(identifier);self.assertTrue(result['cancelled'])
                        else:
                            response=await session.call_tool('cy_cancel_job',{'jobId':identifier})
                            self.assertFalse(response.isError,response)
                            await done(identifier,'Cancelled')
                        # The next exclusive job cannot start until its cancelled
                        # predecessor has terminated the native process tree.
                        result=await done(await submit('project_test',{}));self.assertIn('TEST_OK',result['output'])
                    await asyncio.sleep(4.3)
                    self.assertFalse((self.workspace/'survived').exists(),'Cancelled subprocess tree survived')
        asyncio.run(scenario())

    def test_read_only_information_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        text='/// Combine two values.\nint combine(int left, int right) { return left + right; }\nint main() { const char* emoji = "🛠"; return combine(1, 2); }\n'
        (self.workspace/'main.cpp').write_text(text,'utf-8')
        def point(offset):
            prefix=text[:offset];return {'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'information-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':''})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def invoke(name,inputs,expected='Completed'):
                        response=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(response.isError,response)
                        identifier=json.loads(response.content[0].text)['jobId'];deadline=time.monotonic()+25
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                            if job['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(job['state'],expected,job);return job.get('outputs',{})
                            await asyncio.sleep(.03)
                        self.fail('Information query did not finish')
                    doc=await invoke('documents_read',{'path':'main.cpp'})
                    base={'path':'main.cpp','expectedRevision':doc['revision']}
                    hover=await invoke('language_hover',{**base,'position':point(text.rfind('combine(')+2)})
                    self.assertTrue(hover['available']);self.assertIn('combine',json.dumps(hover))
                    self.assertEqual(hover['positionEncoding'],'utf-16')
                    signature=await invoke('language_signature',{**base,'position':point(text.rfind('2);'))})
                    active=signature['signatures'][signature['activeSignature']]
                    self.assertEqual(active['activeParameter'],1)
                    self.assertEqual(active['parameters'][1]['label'],'int right')
                    await invoke('language_hover',{**base,'position':point(text.index('🛠')) | {'character':point(text.index('🛠'))['character']+1}},'Failed')
                    await invoke('language_signature',{**base,'expectedRevision':doc['revision']+100,'position':point(text.rfind('2);'))},'Failed')
                    denied=await session.call_tool('op_documents_replace',{'parameters':{**base,'text':''}});self.assertTrue(denied.isError)
        asyncio.run(scenario())
        self.assertEqual((self.workspace/'main.cpp').read_text('utf-8'),text)

    def test_completion_import_transaction_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        source='// Unicode 🛠\n\tgre\n'
        (self.workspace/'main.ts').write_text(source,'utf-8')
        (self.workspace/'model.ts').write_text('export function greet(name: string): string { return name; }\n','utf-8')
        (self.workspace/'tsconfig.json').write_text('{"compilerOptions":{"strict":true},"include":["*.ts"]}','utf-8')
        self.client.call('language.configure',{'language':'typescript'})
        async def scenario():
            async def connect(permissions,action):
                parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/('completion-'+('writer' if permissions else 'reader'))),'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
                async with stdio_client(parameters) as (read,write):
                    async with ClientSession(read,write) as session:
                        await session.initialize()
                        async def invoke(name,inputs,expected='Completed'):
                            response=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(response.isError,response)
                            identifier=json.loads(response.content[0].text)['jobId'];deadline=time.monotonic()+35
                            while time.monotonic()<deadline:
                                response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                                if job['state'] in ('Completed','Failed','Cancelled'):
                                    self.assertEqual(job['state'],expected,job);return job.get('outputs',{})
                                await asyncio.sleep(.03)
                            self.fail('Completion job timed out')
                        return await action(session,invoke)
            async def prepare(session,invoke):
                doc=await invoke('documents_read',{'path':'main.ts'})
                deadline=time.monotonic()+25
                while True:
                    completion=await invoke('language_complete',{'path':'main.ts','expectedRevision':doc['revision'],'position':{'line':1,'character':4}})
                    found=next((i for i,item in enumerate(completion['result']['items']) if item['label']=='greet'),None)
                    if found is not None: break
                    self.assertLess(time.monotonic(),deadline,completion);await asyncio.sleep(.1)
                query={'sessionId':completion['sessionId'],'item':found,'expectedRevision':doc['revision']}
                resolved=await invoke('language_completion_resolve',query);self.assertTrue(resolved['item']['additionalTextEdits'])
                prepared=await invoke('language_completion_prepare',query)
                self.assertGreaterEqual(prepared['plan']['editCount'],2)
                preview=await invoke('changes_preview',{'id':prepared['plan']['id'],'path':'main.ts'})
                self.assertIn('import { greet }',preview['after'])
                self.assertEqual((await invoke('documents_read',{'path':'main.ts'}))['text'],source)
                denied=await session.call_tool('op_changes_apply',{'parameters':{'id':prepared['plan']['id']}});self.assertTrue(denied.isError)
                await invoke('language_completion_prepare',{**query,'sessionId':'forged'},'Failed')
                await invoke('language_completion_prepare',{**query,'expectedRevision':doc['revision']+100},'Failed')
                return query,prepared,preview
            query,prepared,preview=await connect('',prepare)
            async def apply(session,invoke):
                await invoke('changes_apply',{'id':prepared['plan']['id']})
                self.assertEqual((await invoke('documents_read',{'path':'main.ts'}))['text'],preview['after'])
                self.assertEqual((self.workspace/'main.ts').read_text('utf-8'),source)
                await invoke('language_completion_prepare',query,'Failed')
                await invoke('changes_undo',{'id':prepared['plan']['id']})
                self.assertEqual((await invoke('documents_read',{'path':'main.ts'}))['text'],source)
            await connect('workspace.write',apply)
        asyncio.run(scenario())
        self.assertEqual((self.workspace/'main.ts').read_text('utf-8'),source)

    def test_code_action_import_transaction_over_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        source="// Unicode 🛠\nconst answer = greet('hello');\n"
        (self.workspace/'main.ts').write_text(source,'utf-8')
        (self.workspace/'model.ts').write_text('export function greet(name: string): string { return name; }\n','utf-8')
        (self.workspace/'tsconfig.json').write_text('{"compilerOptions":{"strict":true},"include":["*.ts"]}','utf-8')
        self.client.call('language.configure',{'language':'typescript'})
        async def scenario():
            async def connect(permissions,action):
                parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/('actions-'+('writer' if permissions else 'reader'))),'mcp'],
                    env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':permissions})
                async with stdio_client(parameters) as (read,write):
                    async with ClientSession(read,write) as session:
                        await session.initialize()
                        names={tool.name for tool in (await session.list_tools()).tools}
                        self.assertTrue({'op_language_code_actions','op_language_code_action_prepare'}.issubset(names))
                        async def invoke(name,inputs,expected='Completed'):
                            response=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(response.isError,response)
                            identifier=json.loads(response.content[0].text)['jobId'];deadline=time.monotonic()+35
                            while time.monotonic()<deadline:
                                response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                                if job['state'] in ('Completed','Failed','Cancelled'):
                                    self.assertEqual(job['state'],expected,job);return job.get('outputs',{})
                                await asyncio.sleep(.03)
                            self.fail('Code action job timed out')
                        return await action(session,invoke)
            async def prepare(session,invoke):
                doc=await invoke('documents_read',{'path':'main.ts'})
                inputs={'path':'main.ts','expectedRevision':doc['revision'],'range':{'start':{'line':1,'character':15},'end':{'line':1,'character':20}},'only':['quickfix']}
                deadline=time.monotonic()+30
                while True:
                    actions=await invoke('language_code_actions',inputs)
                    selected=next((item['index'] for item in actions['actions'] if 'Add import' in item['title'] and item['available']),None)
                    if selected is not None: break
                    self.assertLess(time.monotonic(),deadline,actions);await asyncio.sleep(.1)
                query={'sessionId':actions['sessionId'],'item':selected,'expectedRevision':doc['revision']}
                plan=await invoke('language_code_action_prepare',query)
                self.assertGreater(plan['editCount'],0)
                preview=await invoke('changes_preview',{'id':plan['id'],'path':'main.ts'})
                self.assertIn('import { greet }',preview['after'])
                self.assertEqual((await invoke('documents_read',{'path':'main.ts'}))['text'],source)
                denied=await session.call_tool('op_changes_apply',{'parameters':{'id':plan['id']}});self.assertTrue(denied.isError)
                await invoke('language_code_action_prepare',{**query,'sessionId':'forged'},'Failed')
                await invoke('language_code_action_prepare',{**query,'expectedRevision':doc['revision']+100},'Failed')
                return query,plan,preview
            query,plan,preview=await connect('',prepare)
            async def apply(session,invoke):
                await invoke('changes_apply',{'id':plan['id']})
                self.assertEqual((await invoke('documents_read',{'path':'main.ts'}))['text'],preview['after'])
                self.assertEqual((self.workspace/'main.ts').read_text('utf-8'),source)
                await invoke('language_code_action_prepare',query,'Failed')
                await invoke('changes_undo',{'id':plan['id']})
                self.assertEqual((await invoke('documents_read',{'path':'main.ts'}))['text'],source)
            await connect('workspace.write',apply)
        asyncio.run(scenario())
        self.assertEqual((self.workspace/'main.ts').read_text('utf-8'),source)

    def test_snippet_metadata_over_read_only_official_mcp(self):
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client
        source='// Unicode 🛠\nstruct Widget { int combine(int left, int right); };\nint main() { Widget widget; widget.com }'
        (self.workspace/'main.cpp').write_text(source,'utf-8')
        async def scenario():
            parameters=StdioServerParameters(command=sys.executable,args=[str(APP/'tool.py'),'--data-dir',str(self.directory/'snippet-mcp'),'mcp'],
                env={**os.environ,'CYCODE_BRIDGE':str(self.descriptor),'CYCODE_PERMISSIONS':''})
            async with stdio_client(parameters) as (read,write):
                async with ClientSession(read,write) as session:
                    await session.initialize()
                    async def invoke(name,inputs):
                        response=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(response.isError,response)
                        identifier=json.loads(response.content[0].text)['jobId'];deadline=time.monotonic()+25
                        while time.monotonic()<deadline:
                            response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                            if job['state'] in ('Completed','Failed','Cancelled'):
                                self.assertEqual(job['state'],'Completed',job);return job['outputs']
                            await asyncio.sleep(.03)
                        self.fail('Snippet query timed out')
                    doc=await invoke('documents_read',{'path':'main.cpp'})
                    position={'line':2,'character':len(source.rsplit('\n',1)[1].split(' }')[0])}
                    deadline=time.monotonic()+20
                    while True:
                        completion=await invoke('language_complete',{'path':'main.cpp','expectedRevision':doc['revision'],'position':position})
                        selected=next((i for i,item in enumerate(completion['result']['items']) if 'combine' in item['label'] and item.get('insertTextFormat')==2),None)
                        if selected is not None: break
                        self.assertLess(time.monotonic(),deadline,completion);await asyncio.sleep(.1)
                    prepared=await invoke('language_completion_prepare',{'sessionId':completion['sessionId'],'item':selected,'expectedRevision':doc['revision']})
                    meta=prepared['snippet'];self.assertEqual(meta['positionEncoding'],'utf-8-bytes');self.assertEqual([f['index'] for f in meta['fields']],[1,2])
                    preview=await invoke('changes_preview',{'id':prepared['plan']['id'],'path':'main.cpp'});encoded=preview['after'].encode('utf-8')
                    for field,name in zip(meta['fields'],['left','right']):
                        self.assertIn(name,encoded[field['ranges'][0]['start']:field['ranges'][0]['end']].decode('utf-8'))
                    self.assertTrue(encoded[meta['final']:].startswith(b' }'))
                    self.assertEqual((await invoke('documents_read',{'path':'main.cpp'}))['text'],source)
                    denied=await session.call_tool('op_changes_apply',{'parameters':{'id':prepared['plan']['id']}});self.assertTrue(denied.isError)
        asyncio.run(scenario());self.assertEqual((self.workspace/'main.cpp').read_text('utf-8'),source)

if __name__ == '__main__':
    unittest.main()

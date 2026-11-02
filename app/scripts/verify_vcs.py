"""Verify the native Git panel using real key events and owned repositories."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
workspace=ROOT/'data/validation/vcs-ui'
reports=ROOT/'data/reports'
# Never remove or reuse an unknown repository: every invocation owns a fresh folder.
import tempfile
workspace=Path(tempfile.mkdtemp(prefix='git-',dir=workspace.parent))
reports.mkdir(parents=True,exist_ok=True)
def git(*args,check=True):
    return subprocess.run(['git','-C',str(workspace),'-c','commit.gpgsign=false','-c','user.name=CyCode test','-c','user.email=cycode@example.invalid',*args],check=check,capture_output=True)
def chord(frame,*keys): return [{'frame':frame,'keys':keys,'down':True},{'frame':frame+2,'keys':keys,'down':False}]
def action(frame,text): return chord(frame,'Ctrl','Shift','A')+[{'frame':frame+12,'keys':[],'text':text}]+chord(frame+28,'Enter')
def run(name,events,theme='graphite',language='en'):
    report=reports/f'vcs-{name}.json'
    subprocess.run([str(ROOT/('CyCode.exe' if sys.platform=='win32' else 'CyCode')),'--workspace',str(workspace),'--open','main.cpp',
        '--keymap','rider','--theme',theme,'--language',language,'--test-frames','190','--test-wait-idle','--test-keys',json.dumps(events),
        '--test-report',str(report),'--test-capture',str(reports/f'vcs-{name}.bmp'),'--capture-panel','vcs'],check=True,capture_output=True,timeout=35)
    state=json.loads(report.read_text('utf-8')); assert state['error']=='',state
    assert state['vcs']['details']['repository'],state['vcs']
    return state
try:
    git('init','--initial-branch=main'); git('config','user.name','CyCode test'); git('config','user.email','cycode@example.invalid')
    original='int Answer = 41;\n'
    (workspace/'main.cpp').write_text(original,'utf-8'); git('add','.'); git('commit','-m','Initial source')
    git('branch','feature'); git('remote','add','origin',str(workspace.parent/'gui-local-remote.git'))
    state=run('commit',chord(35,'Ctrl','K'))
    assert 'vcs.changes' in state['ui']['commands'] and state['text']==original,state
    state=run('branches',action(35,'Git branches'))
    assert len(state['vcs']['details']['branches'])==2,state['vcs']
    run('history',action(35,'Git history'))
    state=run('remotes',chord(35,'Ctrl','Shift','K'))
    assert 'vcs.remotes' in state['ui']['commands'],state
    (workspace/'main.cpp').write_text('int Answer = 42;\n','utf-8'); git('stash','push','-m','Work in progress')
    state=run('stashes',action(35,'Git stashes'))
    assert len(state['vcs']['details']['stashes'])==1,state['vcs']
    git('switch','feature'); (workspace/'main.cpp').write_text('int Answer = 43;\n','utf-8'); git('commit','-am','Feature answer')
    git('switch','main'); (workspace/'main.cpp').write_text('int Answer = 44;\n','utf-8'); git('commit','-am','Main answer')
    assert git('merge','--no-edit','feature',check=False).returncode!=0
    conflict=(workspace/'main.cpp').read_bytes()
    for theme,language in [('graphite','en'),('light-grey','fr')]:
        # Command labels follow the selected language, so use the translated action.
        command='Git conflicts' if language=='en' else 'Conflits Git'
        state=run('conflict-'+theme,action(35,command),theme,language)
        versions=state['vcs']['conflict']['versions']
        assert '41' in versions['base'] and '44' in versions['ours'] and '43' in versions['theirs'],versions
        assert (workspace/'main.cpp').read_bytes()==conflict,'Conflict inspection changed disk'
    print('Actual GUI: commit/push shortcuts, branches/history/remotes/stashes, dark/light three-way conflicts passed.',flush=True)
finally:
    # Only this invocation's known temporary fixture is eligible for cleanup.
    import os, stat
    def remove_readonly(function,path,info):
        os.chmod(path,stat.S_IWRITE); function(path)
    shutil.rmtree(workspace,onerror=remove_readonly)

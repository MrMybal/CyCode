"""Exercise real project shortcuts and Cargo debugging; all UI windows stay hidden."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
REPORTS=ROOT/'data/reports/project-tasks'

def chord(frame,*keys,ready=None):
    down={'frame':frame,'keys':keys,'down':True}
    if ready: down['waitForCommand']=ready
    return [down,{'frame':frame+2,'keys':keys,'down':False}]

def run(root,name,source,events,theme='graphite',language='en',frames=230,panel=None):
    report=REPORTS/(name+'.json')
    argv=[str(ROOT/'CyCode.exe'),'--workspace',str(root),'--open',source,'--keymap','rider',
        '--theme',theme,'--language',language,'--show-project-tasks','--test-hidden',
        '--test-frames',str(frames),'--test-wait-idle','--test-keys',json.dumps(events),
        '--test-report',str(report),'--test-capture',str(REPORTS/(name+'.bmp'))]
    if panel: argv+=['--capture-panel',panel]
    result=subprocess.run(argv,capture_output=True,timeout=90)
    assert result.returncode==0,(name,result.returncode,result.stderr.decode(errors='replace'))
    assert report.is_file(),(name,'GUI ended before producing its report; inspect application/protection logs.',
        'executable still present: '+str((ROOT/'CyCode.exe').is_file()),result.stderr.decode(errors='replace'))
    state=json.loads(report.read_text('utf-8'))
    assert state['hiddenWindow'] and not state['error'],state
    assert state['projectActive'] and state['projectTasks']['available'],state
    return state

def main():
    REPORTS.mkdir(parents=True,exist_ok=True)
    base=ROOT/'data/validation';base.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='project-tasks été-',dir=base) as folder:
        base=Path(folder)
        for kind in ('cargo','npm','java'):
            root=base/kind;root.mkdir();(root/'.cycode').mkdir()
            profile={'system':kind,'args':['a & b é'],'offline':True}
            if kind=='cargo':
                (root/'src').mkdir()
                (root/'Cargo.toml').write_text('[package]\nname="workflow_ui"\nversion="0.1.0"\nedition="2021"\n','utf-8')
                source='src/main.rs'
                text='#[inline(never)]\nfn compute(value: i32) -> i32 { value + 1 }\nfn main() {\n let value = 41;\n let result = compute(value);\n println!("RESULT={} ARG={}", result, std::env::args().nth(1).unwrap_or_default());\n}\n'
            elif kind=='npm':
                (root/'package.json').write_text(json.dumps({'name':'workflow-ui','private':True,'scripts':{
                    'build':'node build.cjs','test':'node test.cjs','start':'node app.cjs'}}),'utf-8')
                (root/'build.cjs').write_text("require('node:fs').writeFileSync('built','yes');console.log('BUILD_OK');",'utf-8')
                (root/'test.cjs').write_text("console.log('TEST_OK');",'utf-8')
                source='app.cjs';text="console.log('RESULT=42 ARG='+process.argv[2]);\n"
            else:
                source='Main.java';text='public class Main { public static void main(String[] args) { System.out.println("RESULT=42 ARG="+args[0]); } }\n'
            (root/source).write_text(text,'utf-8')
            (root/'.cycode/project-tasks.json').write_text(json.dumps(profile),'utf-8')
            events=chord(35,'Ctrl','F9',ready='build.run')+chord(100,'Shift','F10',ready='run.start')
            state=run(root,kind+'-run',source,events)
            assert state['ui']['commands']==['build.run','run.start'],state['ui']
            assert 'RESULT=42 ARG=a & b é' in '\n'.join(state['terminal']['lines']),state
            assert (root/source).read_text('utf-8')==text,'Execution changed source text'
            if kind!='npm': assert state['projectTasks']['artifacts'],state
            print(kind+': actual Ctrl+F9 / Shift+F10 and Unicode run arguments passed (hidden)',flush=True)
            if kind=='cargo':
                (root/'.cycode/launch.json').write_text(json.dumps({'profile':{'program':'','cwd':'.','args':[],
                    'env':{},'adapter':'automatic','runtime':'native','stopOnEntry':False,'terminal':'integrated',
                    'buildBeforeLaunch':True},'breakpoints':{source:[{'line':5}]}}),'utf-8')
                events=chord(35,'Shift','F9',ready='debug.start')+[{'frame':100,'keys':[],'waitForCommand':'debug.next'}]
                state=run(root,'cargo-breakpoint',source,events,panel='debugger')
                assert state['debug']['state']=='stopped' and not state['debug']['error'],state['debug']
                assert any(v['name']=='value' and v['value']=='41' for v in state['debug']['variables']),state['debug']
                assert state['active']==source and state['cursor']['line']==4,state
                state=run(root,'cargo-step',source,events+chord(160,'F8'),frames=250,panel='debugger')
                assert state['debug']['state']=='stopped',state['debug']
                assert any(v['name']=='result' and v['value']=='42' for v in state['debug']['variables']),state['debug']
                print('Cargo: actual build-before-debug, source breakpoint, locals and F8 step passed (hidden)',flush=True)
            themes={'cargo':['graphite','mid-grey'],'npm':['light-grey','amber-night'],'java':['violet','navy']}
            for theme in themes[kind]:
                state=run(root,kind+'-panel-'+theme,source,[],theme=theme,language='fr' if theme in ('light-grey','navy') else 'en',frames=85,panel='projectTasks')
                assert state['theme']==theme,state
    print('Project task UI acceptance passed; no visible windows were opened.',flush=True)

if __name__=='__main__': main()

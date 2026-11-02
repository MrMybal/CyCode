"""Actual IDE language UI checks. Reader checks live in its own repository. Every native window stays hidden."""
import json
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
SAMPLES={
    'main.rs':('fn greet(value: i32) -> i32 { value + 1 }\nfn main() { gre }\n',1,15,'greet'),
    'Main.java':('class Main {\n static int greet(int value) { return value + 1; }\n void run() { gre }\n}\n',2,17,'greet'),
    'main.js':("const message = 'hello';\nmessage.toU\n",1,11,'toUpperCase'),
    'main.ts':("const message: string = 'hello';\nmessage.toU\n",1,11,'toUpperCase'),
    'index.html':('<!-- embedded languages -->\n<style>.card { color: #ffffff; }</style>\n<script>const value = `hello ${name}`;</script>\n<main><di',3,9,'div'),
    'style.css':('/* stylesheet */\n.card { col',1,11,'color'),
}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--language',choices=['rust','java','javascript','typescript','html','css'],action='append')
    options=parser.parse_args()
    base=ROOT/'data/validation';base.mkdir(parents=True,exist_ok=True)
    reports=ROOT/'data/reports/languages';reports.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='languages-',dir=base) as folder:
        root=Path(folder)
        (root/'Cargo.toml').write_text('[package]\nname="cycode_ui"\nversion="0.1.0"\nedition="2021"\n')
        (root/'src').mkdir()
        (root/'tsconfig.json').write_text(json.dumps({'compilerOptions':{'target':'ES2020','allowJs':True},'include':['*.ts','*.js']}))
        files=[]
        for index,(name,(text,line,character,expected)) in enumerate(SAMPLES.items()):
            key={'main.rs':'rust','Main.java':'java','main.js':'javascript','main.ts':'typescript','index.html':'html','style.css':'css'}[name]
            if options.language and key not in options.language: continue
            path=('src/'+name) if name.endswith('.rs') else name
            file=root/path;file.write_text(text,'utf-8');files.append(file)
            report=reports/(name+'.json')
            action={'path':path,'position':{'line':line,'character':character},'acceptFirst':True}
            theme='graphite' if index%2==0 else 'light-grey'
            command=[str(ROOT/'CyCode.exe'),'--workspace',str(root),'--open',path,'--theme',theme,
                '--test-hidden','--test-frames','180','--test-wait-idle','--test-completion',json.dumps(action),
                '--test-report',str(report),'--test-capture',str(reports/(name+'.bmp'))]
            result=subprocess.run(command,capture_output=True,timeout=80)
            assert result.returncode==0,(name,result.returncode,result.stderr.decode(errors='replace'))
            state=json.loads(report.read_text('utf-8'))
            assert state['hiddenWindow'] and not state['error'],state
            assert state['syntax']['spans']>0 and not state['syntax']['truncated'],state
            assert state['completionTest']['phase']==5,state
            assert expected in state['text'].splitlines()[line] and state['dirty'],state
            assert file.read_text('utf-8')==text,'Completion wrote to disk'
            print(name+': actual Ctrl+Space/Enter insertion and shared syntax passed (hidden)',flush=True)
    print('Language UI checks passed without visible application windows.')

if __name__=='__main__': main()

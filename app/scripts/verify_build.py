"""Configure/build/run the supplied trusted C++ example through native IDE services."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--cli',type=Path,required=True)
args=parser.parse_args()
executable=str(args.cli.resolve())
with tempfile.TemporaryDirectory(prefix='cycode-build-') as directory:
    workspace=Path(directory)
    for source in (ROOT/'app/examples/hello').iterdir():
        if source.is_file():
            shutil.copy2(source,workspace/source.name)
    reports={}
    for method,parameters in [('build.configure',{}),('build.run',{'configuration':'Release'})]:
        process=subprocess.run([executable,str(workspace),method,json.dumps(parameters)],capture_output=True,timeout=200,check=True)
        result=json.loads(process.stdout)
        if result['exitCode']!=0:
            raise RuntimeError(result['output'])
        reports[method]={'exitCode':result['exitCode'],'timedOut':result['timedOut']}
    binary=workspace/'build/Release/hello.exe' if sys.platform=='win32' else workspace/'build/hello'
    output=subprocess.check_output([str(binary)]).decode('utf-8')
    if output.strip()!='Hello, CyCode!':
        raise RuntimeError('Unexpected example output')
    reports['programOutput']=output.strip()
    path=ROOT/'data/reports/build-example.json'
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(reports,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(reports,indent=2))

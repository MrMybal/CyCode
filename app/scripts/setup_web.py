"""Install locked web language servers locally; never runs package lifecycle scripts."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
def ensure_web():
    node=shutil.which('node')
    if not node: raise RuntimeError('Install Node.js 20+ to enable the web language servers.')
    version=subprocess.check_output([node,'--version'],text=True).strip()
    if int(version.lstrip('v').split('.')[0])<20: raise RuntimeError('Web language servers require Node.js 20+.')
    source=ROOT/'app/web-languages'
    destination=ROOT/'runtime/tools/web-languages'
    destination.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256((source/'package-lock.json').read_bytes()).hexdigest()
    stamp=destination/'installed.json'
    entries=['typescript-language-server/lib/cli.mjs','vscode-langservers-extracted/bin/vscode-html-language-server',
             'vscode-langservers-extracted/bin/vscode-css-language-server','typescript/lib/tsserver.js']
    if stamp.exists():
        saved=json.loads(stamp.read_text('utf-8'))
        if saved.get('lockSha256')==digest and all((destination/'node_modules'/entry).is_file() and
            saved.get('entrySha256',{}).get(entry)==hashlib.sha256((destination/'node_modules'/entry).read_bytes()).hexdigest() for entry in entries):
            return
    for name in ['package.json','package-lock.json']: shutil.copy2(source/name,destination/name)
    npm=Path(node).parent/'node_modules/npm/bin/npm-cli.js'
    if not npm.is_file():
        found=shutil.which('npm')
        if found and Path(found).resolve().suffix=='.js': npm=Path(found).resolve()
    if not npm.is_file(): raise RuntimeError('Cannot locate the npm CLI beside Node.js.')
    subprocess.run([node,str(npm),'ci','--prefix='+str(destination),'--ignore-scripts','--no-audit','--no-fund',
                    '--registry=https://registry.npmjs.org','--cache='+str(ROOT/'runtime/npm-cache')],cwd=destination,check=True)
    hashes={entry:hashlib.sha256((destination/'node_modules'/entry).read_bytes()).hexdigest() for entry in entries}
    stamp.write_text(json.dumps({'lockSha256':digest,'entrySha256':hashes,'nodeVersion':version},indent=2)+'\n','utf-8')

if __name__=='__main__': ensure_web()

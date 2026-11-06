"""Resolve the locked private CyDoc generator without changing its developer checkout."""
from pathlib import Path
import io, json, shutil, subprocess, tarfile

def resolve(root, lock, supplied=None):
    node=shutil.which('node')
    if not node or int(subprocess.check_output([node,'--version'],text=True).strip().lstrip('v').split('.')[0])<22:
        raise RuntimeError('Documentation generation requires Node.js 22 or newer')
    revision=lock['revision']
    target=root/'runtime/deps'/('cydoc-'+revision[:7])
    marker=target/'.cydoc-revision'
    if marker.exists():
        if marker.read_text().strip()!=revision: raise RuntimeError('CyDoc cache revision differs from the lock')
    elif supplied:
        source=supplied.resolve()
        git=['git','-c',f'safe.directory={source.as_posix()}','-C',str(source)]
        if subprocess.check_output(git+['rev-parse','HEAD'],text=True).strip()!=revision:
            raise RuntimeError('The supplied CyDoc checkout must match the documentation lock')
        if subprocess.check_output(git+['status','--porcelain'],text=True).strip():
            raise RuntimeError('The supplied CyDoc checkout has local source changes')
        if target.exists(): raise RuntimeError('Preserve the existing unrecognized CyDoc cache')
        target.mkdir(parents=True)
        with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(git+['archive',revision]))) as archive:
            for member in archive.getmembers():
                path=Path(member.name)
                if path.is_absolute() or '..' in path.parts or member.issym() or member.islnk():
                    raise RuntimeError('Unsafe CyDoc archive member')
            archive.extractall(target,filter='data')
        marker.write_text(revision+'\n')
        if (source/'node_modules').is_dir(): shutil.copytree(source/'node_modules',target/'node_modules')
    else:
        if target.exists(): raise RuntimeError('Preserve the existing unrecognized CyDoc cache')
        target.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['git','clone','--no-checkout',lock['repository'],str(target)],check=True)
        subprocess.run(['git','-C',str(target),'checkout','--detach',revision],check=True)
        marker.write_text(revision+'\n')
    if not (target/'node_modules/typescript/bin/tsc').is_file():
        npm=shutil.which('npm.cmd') or shutil.which('npm')
        if not npm: raise RuntimeError('npm is needed to prepare the CyDoc generator')
        subprocess.run([npm,'ci','--cache',str(root/'runtime/npm-cache')],cwd=target,check=True)
    if not (target/'dist/index.js').is_file():
        subprocess.run([node,str(target/'node_modules/typescript/bin/tsc'),'-p',str(target/'tsconfig.json')],check=True)
    package=json.loads((target/'package.json').read_text(encoding='utf-8'))
    if package['version']!=lock['version']: raise RuntimeError('CyDoc version differs from the lock')
    return node,target

"""Compose the installer from reviewed package bytes; never execute the payload."""
from pathlib import Path
import os, re, shutil, subprocess, tempfile
from release_package import package_path

ROOT=Path(__file__).resolve().parents[2]

def cmake_for(build):
    found=shutil.which('cmake')
    if found: return found
    return re.search(r'^CMAKE_COMMAND:INTERNAL=(.+)$',(build/'CMakeCache.txt').read_text('utf-8'),re.M).group(1).strip()

def system_runtime(build, configuration, output):
    if configuration != 'Release': raise ValueError('Only Release distributions are supported')
    output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='msvc-',dir=output) as folder:
        stage=Path(folder)
        subprocess.run([cmake_for(build),'--install',str(build),'--config',configuration,'--prefix',str(stage)],check=True)
        result={p.name:p.read_bytes() for p in stage.glob('*.dll')}
        if not {'msvcp140.dll','vcruntime140.dll'} <= result.keys(): raise RuntimeError('Standalone MSVC runtime is missing')
        if not all(package_path(name) for name in result): raise RuntimeError('Unexpected system runtime file')
        return result

def find_iscc(explicit):
    candidates=[explicit,shutil.which('ISCC')]
    for key in ('ProgramFiles','ProgramFiles(x86)','LOCALAPPDATA'):
        for suffix in ('Inno Setup 6/ISCC.exe','Inno Setup 7/ISCC.exe','Programs/Inno Setup 6/ISCC.exe'):
            candidates.append(Path(os.environ.get(key,'.'))/suffix)
    found=next((Path(p) for p in candidates if p and Path(p).is_file()),None)
    if not found: raise RuntimeError('Inno Setup 6.7+ ISCC.exe is required')
    return found

def compile_installer(entries, version, output, explicit=None):
    compiler=find_iscc(explicit)
    if not all(package_path(name) for name in entries): raise RuntimeError('Unsafe installer payload')
    with tempfile.TemporaryDirectory(prefix='installer-',dir=output) as temporary:
        stage=Path(temporary)/'files'
        for name,data in entries.items():
            target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        (stage/'installed.flag').write_bytes(b'CyCode per-user installation\n')
        subprocess.run([str(compiler),f'/DAppVersion={version}',f'/DSourceDir={stage}',
                        f'/DOutputDir={output}',str(ROOT/'app/packaging/windows.iss')],check=True)
    result=output/f'CyCode-{version}-windows-x64-setup.exe'
    if not result.is_file() or result.stat().st_size > 256*1024*1024: raise RuntimeError('Missing or oversized installer')
    return result

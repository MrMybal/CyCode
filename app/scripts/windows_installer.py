"""Compose the installer from reviewed package bytes; never execute the payload."""
from pathlib import Path
import os, re, shutil, subprocess, tempfile
from release_package import DEFAULT_PRODUCT, package_path, validate_payload

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

def compile_installer(entries, version, output, explicit=None, *, product=DEFAULT_PRODUCT, script=None, required=()):
    validate_payload(entries, version, product=product, required=required)
    if script is None and not product.accepts_legacy_metadata:
        raise ValueError('A consumer product requires its own reviewed installer script')
    script=Path(script) if script is not None else ROOT/'app/packaging/windows.iss'
    if not script.is_file(): raise ValueError('Installer script is missing')
    compiler=find_iscc(explicit)
    with tempfile.TemporaryDirectory(prefix='installer-',dir=output) as temporary:
        stage=Path(temporary)/'files'
        for name,data in entries.items():
            target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        (stage/product.installation_marker).write_bytes(product.installation_marker_bytes)
        subprocess.run([str(compiler),f'/DAppVersion={version}',f'/DSourceDir={stage}',
                        f'/DOutputDir={output}',f'/DUpdateProduct={product.product}',
                        f'/DUpdateRepository={product.repository}',f'/DInstallMarker={product.installation_marker}',
                        str(script)],check=True)
    result=output/f'{product.product}-{version}-windows-x64-setup.exe'
    if not result.is_file() or result.stat().st_size > 256*1024*1024: raise RuntimeError('Missing or oversized installer')
    return result

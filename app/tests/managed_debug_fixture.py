"""An owned console app with portable PDBs; restores without network package sources."""
from pathlib import Path
import shutil
import subprocess

def create_managed_debug_fixture(root):
    root = Path(root)
    root.mkdir(parents=True,exist_ok=True)
    source = Path(__file__).with_name('managed_debuggee.cs').read_text('utf-8')
    (root/'Program.cs').write_text(source,'utf-8')
    (root/'global.json').write_text('{"sdk":{"version":"9.0.100","rollForward":"latestFeature"}}','utf-8')
    (root/'NuGet.Config').write_text('<configuration><packageSources><clear /></packageSources></configuration>','utf-8')
    project = '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><DebugType>portable</DebugType><DebugSymbols>true</DebugSymbols><Optimize>false</Optimize></PropertyGroup></Project>'
    (root/'Managed.csproj').write_text(project,'utf-8')
    dotnet = shutil.which('dotnet') or 'C:/Program Files/dotnet/dotnet.exe'
    result = subprocess.run([dotnet,'build','Managed.csproj','-c','Debug','-p:Platform=AnyCPU','--configfile','NuGet.Config','--nologo'],cwd=root,capture_output=True,timeout=45)
    if result.returncode:
        raise RuntimeError((result.stdout+result.stderr).decode('utf-8',errors='replace'))
    program = root/'bin/Debug/net8.0/Managed.dll'
    breakpoint = next(i for i,line in enumerate(source.splitlines(),1) if 'BREAK_HERE' in line)
    return program,source,breakpoint

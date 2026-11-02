"""Owned, offline-restored two-project .NET fixture used by MCP/desktop acceptance."""
from pathlib import Path
import shutil
import subprocess

LIBRARY = 'namespace Demo;\npublic class Counter { public int Value; public int VisibleMember; }\n'
CONSUMER = 'using Demo;\npublic class Consumer { public int Read(Counter c) { return c.Value; } }\n'

def create_csharp_fixture(root):
    root = Path(root)
    dotnet = shutil.which('dotnet') or str(Path('C:/Program Files/dotnet/dotnet.exe'))
    for name in ('Library','App'):
        (root/name).mkdir(parents=True, exist_ok=True)
    (root/'global.json').write_text('{"sdk":{"version":"9.0.100","rollForward":"latestFeature"}}', 'utf-8')
    (root/'NuGet.Config').write_text('<configuration><packageSources><clear /></packageSources></configuration>', 'utf-8')
    (root/'Library/Library.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework></PropertyGroup></Project>', 'utf-8')
    (root/'App/App.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework></PropertyGroup><ItemGroup><ProjectReference Include="../Library/Library.csproj" /></ItemGroup></Project>', 'utf-8')
    (root/'Library/Counter.cs').write_text(LIBRARY, 'utf-8')
    (root/'App/Consumer.cs').write_text(CONSUMER, 'utf-8')
    for arguments in (['new','sln','--name','Managed'], ['sln','Managed.sln','add','Library/Library.csproj','App/App.csproj'], ['restore','Managed.sln','--configfile','NuGet.Config']):
        result = subprocess.run([dotnet,*arguments], cwd=root, capture_output=True, timeout=35)
        if result.returncode:
            raise RuntimeError(result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace'))
    return root/'Managed.sln'

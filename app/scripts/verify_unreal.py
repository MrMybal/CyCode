"""Real UBT/UHT/clangd acceptance in an owned workspace. Never builds the user's project."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'app'))
from tool import NativeClient

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--cli', type=Path, default=ROOT / ('runtime/build-vs/Release/cycode_cli.exe' if sys.platform == 'win32' else 'runtime/build/cycode_cli'))
    args = parser.parse_args()
    cli = args.cli.resolve()
    engine = args.engine.resolve()
    if not (engine / 'Engine/Build/Build.version').is_file():
        raise RuntimeError('Select an installed Unreal engine root')
    root = ROOT / 'data/validation/unreal-acceptance'
    source = root / 'Source/CyCodeFixture'
    source.mkdir(parents=True, exist_ok=True)
    descriptor = root / 'CyCodeFixture.uproject'
    descriptor.write_text(json.dumps({'FileVersion': 3, 'Modules': [{'Name': 'CyCodeFixture', 'Type': 'Runtime', 'LoadingPhase': 'Default'}]}), encoding='utf-8')
    for suffix, kind in (('', 'Game'), ('Editor', 'Editor')):
        name = 'CyCodeFixture' + suffix
        (root / f'Source/{name}.Target.cs').write_text('using UnrealBuildTool;\n' +
            f'public class {name}Target : TargetRules {{ public {name}Target(TargetInfo Target) : base(Target) {{ Type = TargetType.{kind}; DefaultBuildSettings = BuildSettingsVersion.V4; ExtraModuleNames.Add("CyCodeFixture"); }} }}\n', encoding='utf-8')
    (source / 'CyCodeFixture.Build.cs').write_text('using UnrealBuildTool;\npublic class CyCodeFixture : ModuleRules { public CyCodeFixture(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine"}); } }\n', encoding='utf-8')
    (source / 'CyCodeFixture.cpp').write_text('#include "Modules/ModuleManager.h"\nIMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CyCodeFixture, "CyCodeFixture");\n', encoding='utf-8')
    (source / 'FixtureTypes.h').write_text('#pragma once\n#include "CoreMinimal.h"\n#include "FixtureTypes.generated.h"\nUSTRUCT()\nstruct FFixtureThing { GENERATED_BODY() UPROPERTY() int32 Value = 42; };\n', encoding='utf-8')
    text = '#include "FixtureTypes.h"\nint CyCodeFixtureAnswer() { FFixtureThing Thing; TArray<int32> Values; Values.Add(Thing.Value); return Values[0]; }\n'
    (source / 'Widget.cpp').write_text(text, encoding='utf-8')
    reports = ROOT / 'data/reports'
    reports.mkdir(parents=True, exist_ok=True)
    def invoke(method, params=None):
        response = subprocess.run([str(cli), str(descriptor), method, json.dumps(params or {})], capture_output=True, text=True, encoding='utf-8', check=True, timeout=195)
        result = json.loads(response.stdout)
        (reports / (method.replace('.', '-') + '-acceptance.json')).write_text(response.stdout, encoding='utf-8')
        if result.get('exitCode', 0) != 0:
            raise RuntimeError(f'{method} failed; see data/reports/{method.replace(".", "-")}-acceptance.json')
        return result
    metadata = invoke('unreal.inspect', {'engineRoot': str(engine)})
    if not metadata['ready'] or metadata['profile']['target'] != 'CyCodeFixtureEditor':
        raise RuntimeError('Unreal engine/target discovery failed')
    # Prepare analysis before any build: UHT must be sufficient on a fresh project.
    prepared = invoke('unreal.prepare', {'engineRoot': str(engine)})
    if not list((root / 'Intermediate').rglob('FixtureTypes.generated.h')):
        raise RuntimeError('Analysis preparation did not run UnrealHeaderTool')
    for method in ('unreal.generate', 'unreal.build', 'unreal.database'):
        result = invoke(method, {'engineRoot': str(engine)})
        print(f'{method}: passed', flush=True)
    if sys.platform == 'win32' and not (root / 'Binaries/Win64/UnrealEditor-CyCodeFixture.dll').exists():
        raise RuntimeError('Missing compiled editor module')
    if not list((root / 'Intermediate').rglob('FixtureTypes.generated.h')):
        raise RuntimeError('UnrealHeaderTool did not generate the reflection header')
    commands = json.loads((root / result['project']['compilationDatabase']['path']).read_text('utf-8'))
    if not any('Widget.cpp' in entry['file'] for entry in commands):
        raise RuntimeError('Unreal compilation database does not describe the source')
    bridge = root / 'native-test.json'
    bridge.unlink(missing_ok=True)
    process = subprocess.Popen([str(cli), str(descriptor), '--serve', str(bridge)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 10
        while not bridge.exists() and time.monotonic() < deadline and process.poll() is None:
            time.sleep(.03)
        client = NativeClient(bridge)
        document = client.call('documents.read', {'path': 'Source/CyCodeFixture/Widget.cpp'})
        at = text.index('Value);')
        result = client.call('language.definition', {'path': document['path'], 'expectedRevision': document['revision'],
            'position': {'line': text[:at].count('\n'), 'character': at - text.rfind('\n', 0, at) - 1}})
        if not any(item['path'] == 'Source/CyCodeFixture/FixtureTypes.h' for item in result['locations']):
            raise RuntimeError('Unreal generated-header/include/macro navigation failed')
        (reports / 'unreal-clangd-acceptance.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    finally:
        process.communicate(b'\n' if process.poll() is None else None, timeout=15)
    print('Passed: .uproject discovery, actual UBT generation/build/index, UHT reflection header, shared clangd definition of a USTRUCT member.')

if __name__ == '__main__':
    main()

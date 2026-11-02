"""Open a fresh owned Unreal project and exercise the GUI's actual completion popup."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--fresh', action='store_true')
    args = parser.parse_args()
    root = ROOT / 'data/validation' / ('unreal-completion-' + str(time.time_ns()) if args.fresh else 'unreal-completion')
    source = root / 'Source/CyCodeCompletion'
    source.mkdir(parents=True, exist_ok=True)
    project = root / 'CyCodeCompletion.uproject'
    project.write_text(json.dumps({'FileVersion': 3, 'EngineAssociation': '5.3', 'Modules': [{'Name': 'CyCodeCompletion', 'Type': 'Runtime'}]}), 'utf-8')
    for suffix, kind in (('', 'Game'), ('Editor', 'Editor')):
        name = 'CyCodeCompletion' + suffix
        (root / f'Source/{name}.Target.cs').write_text('using UnrealBuildTool;\n' +
            f'public class {name}Target : TargetRules {{ public {name}Target(TargetInfo Target) : base(Target) {{ Type = TargetType.{kind}; DefaultBuildSettings = BuildSettingsVersion.V4; ExtraModuleNames.Add("CyCodeCompletion"); }} }}\n', 'utf-8')
    (source / 'CyCodeCompletion.Build.cs').write_text('using UnrealBuildTool;\npublic class CyCodeCompletion : ModuleRules { public CyCodeCompletion(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine"}); } }\n', 'utf-8')
    (source / 'CyCodeCompletion.cpp').write_text('#include "Modules/ModuleManager.h"\nIMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CyCodeCompletion, "CyCodeCompletion");\n', 'utf-8')
    (source / 'FixtureTypes.h').write_text('#pragma once\n#include "CoreMinimal.h"\n#include "FixtureTypes.generated.h"\nUSTRUCT()\nstruct FFixtureThing { GENERATED_BODY() UPROPERTY() int32 Value = 42; };\n', 'utf-8')
    probes = {
        'actor': ('#include "FixtureTypes.h"\n#include "GameFramework/Actor.h"\nvoid Probe(AActor* Actor) { Actor->GetAct; }\n', 'GetAct', 'GetActorLocation'),
        'reflection': ('#include "FixtureTypes.h"\nvoid Probe() { FFixtureThing Thing; Thing.Val; }\n', 'Thing.Val', 'Value'),
    }
    reports = ROOT / 'data/reports'
    reports.mkdir(parents=True, exist_ok=True)
    # Tell discovery which engine to use, without changing global user preferences.
    (root / '.cycode').mkdir(exist_ok=True)
    (root / '.cycode/unreal.json').write_text(json.dumps({'engineRoot': str(args.engine.resolve())}), 'utf-8')
    exe = ROOT / ('CyCode.exe' if sys.platform == 'win32' else 'CyCode')
    for name, (text, marker, expected) in probes.items():
        file = source / 'Probe.cpp'
        file.write_text(text, 'utf-8')
        offset = text.index(marker) + len(marker)
        position = {'line': text[:offset].count('\n'), 'character': offset - text.rfind('\n', 0, offset) - 1}
        report = reports / f'unreal-completion-{name}.json'
        command = [str(exe), '--workspace', str(project), '--test-frames', '150', '--test-wait-idle',
            '--test-completion', json.dumps({'path': 'Source/CyCodeCompletion/Probe.cpp', 'position': position, 'acceptFirst': name == 'reflection'}),
            '--test-report', str(report), '--test-capture', str(reports / f'unreal-completion-{name}.bmp')]
        subprocess.run(command, timeout=205, check=True, capture_output=True)
        state = json.loads(report.read_text('utf-8'))
        assert state['unrealAnalysis'] == 'ready', state
        assert state['error'] == '', state['error']
        if name == 'reflection': assert 'Thing.Value;' in state['text'], state['text']
        else: assert any(expected in item for item in state['completionSuggestions']), state['completionSuggestions']
        assert file.read_text('utf-8') == text, 'Completion test changed disk source'
        assert list((root / 'Intermediate').rglob('FixtureTypes.generated.h')), 'Missing UHT reflection header'
        print(name + ': actual Unreal GUI completion passed', flush=True)
    cli = ROOT / ('runtime/build-vs/Release/cycode_cli.exe' if sys.platform == 'win32' else 'runtime/build/cycode_cli')
    def prepare():
        reply = subprocess.run([str(cli), str(project), 'unreal.prepare', '{}'], timeout=195, check=True, capture_output=True, text=True, encoding='utf-8')
        result = json.loads(reply.stdout)
        assert result['exitCode'] == 0, result['output']
        return result
    assert prepare()['cached'] is True, 'Unchanged project should reuse its analysis cache'
    header = next((root / 'Intermediate').rglob('FixtureTypes.generated.h')).resolve()
    assert header.is_relative_to((ROOT / 'data/validation').resolve()), 'Generated header is outside our owned test directory'
    previous = header.read_bytes()
    try:
        header.unlink()
        assert prepare()['cached'] is False, 'A missing generated header must invalidate the cache'
        assert header.exists(), 'UnrealHeaderTool did not recreate the missing header'
    finally:
        if not header.exists(): header.write_bytes(previous)
    print('Unreal cache reuse and regeneration of a missing UHT header: passed', flush=True)
    print('Fresh project analysis, UHT reflection, engine includes and native GUI completions passed.')

if __name__ == '__main__':
    main()

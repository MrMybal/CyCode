"""Validate native CMake/Python update identities without building or running payloads."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def verify(cmake):
    source = Path(__file__).resolve().parents[2]
    scenarios = [
        ('legacy', {}, True),
        ('ide', dict(PRODUCT='CyCodeIDE', REPOSITORY='MrMybal/CyCodeIDE', VERSION='0.2.3'), True),
        ('fork', dict(PRODUCT='CyCode', REPOSITORY='OtherOwner/CyCode', VERSION='1.0.0'), True),
        ('future', dict(PRODUCT='Future_Reader', REPOSITORY='Owner/Reader', VERSION='0.0.1'), True),
        ('same-target-again', dict(PRODUCT='CyCodeIDE', REPOSITORY='MrMybal/CyCodeIDE', VERSION='0.2.3', RECONFIGURE='CyCodeIDE'), True),
        ('different-target-profile', dict(PRODUCT='CyCodeIDE', REPOSITORY='MrMybal/CyCodeIDE', VERSION='0.2.3', RECONFIGURE='CyCodeReader'), False),
        ('missing-repository', dict(PRODUCT='CyCodeIDE', VERSION='0.2.3'), False),
        ('missing-version', dict(PRODUCT='CyCodeIDE', REPOSITORY='MrMybal/CyCodeIDE'), False),
    ]
    baseline = dict(PRODUCT='CyCodeIDE', REPOSITORY='MrMybal/CyCodeIDE', VERSION='0.2.3')
    for field, values in (
        ('PRODUCT', ('../IDE', 'IDE.exe', 'IDE;Other', 'NUL', 'LPT1', 'IDE\n', 'éIDE', 'X' * 65)),
        ('REPOSITORY', ('https://github.com/Owner/Repo', '../Repo', 'Owner/Repo/Other', 'Owner/Repo\n', 'Owner/Repo;token')),
        ('VERSION', ('01.2.3', '1.2', '1.2.65536', '1.2.3-beta', '1.2.3\n', '1.2.99999999999999999999')),
    ):
        scenarios.extend((field.lower() + '-' + str(i), dict(baseline, **{field: value}), False)
                         for i, value in enumerate(values))
    results = []
    with tempfile.TemporaryDirectory(prefix='cycode-update-profile-') as temporary:
        root = Path(temporary)
        project = root / 'source'
        project.mkdir()
        (project / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.24)\n'
            'project(UpdateProfileContract VERSION 0.2.3 LANGUAGES NONE)\n'
            'add_library(cycode_updates INTERFACE)\n'
            f'include("{source.as_posix()}/cmake/CyCodeUpdates.cmake")\n'
            'cycode_configure_updates("${CMAKE_CURRENT_BINARY_DIR}/generated")\n'
            'if(DEFINED CYCODE_UPDATES_RECONFIGURE)\n'
            '  set(CYCODE_UPDATES_PRODUCT "${CYCODE_UPDATES_RECONFIGURE}")\n'
            '  set(CYCODE_UPDATES_REPOSITORY "MrMybal/${CYCODE_UPDATES_RECONFIGURE}")\n'
            '  cycode_configure_updates("${CMAKE_CURRENT_BINARY_DIR}/generated")\n'
            'endif()\n', encoding='utf-8')
        for label, fields, accepted in scenarios:
            build = root / label
            result = subprocess.run([str(cmake), '-S', str(project), '-B', str(build),
                *[f'-DCYCODE_UPDATES_{key}={value}' for key, value in fields.items()]],
                capture_output=True, text=True, timeout=30)
            if (result.returncode == 0) != accepted:
                raise RuntimeError(f'Update profile validation mismatch: {label}\n{result.stdout}\n{result.stderr}')
            if accepted:
                header = (build / 'generated/cycode/UpdateProductConfig.h').read_text('utf-8')
                product = fields.get('PRODUCT', 'CyCode')
                repository = fields.get('REPOSITORY', 'MrMybal/CyCode')
                legacy = product == 'CyCode' and repository == 'MrMybal/CyCode'
                expected = {'PRODUCT': product, 'REPOSITORY': repository,
                            'VERSION': fields.get('VERSION', '0.2.3'),
                            'APPLICATION': product + '.exe', 'HELPER': product + 'Updater.exe',
                            'MARKER': 'installed.flag' if legacy else product + '-installed.flag',
                            'TOKEN_ENV': product.upper() + '_UPDATE_TOKEN'}
                for key, value in expected.items():
                    if f'#define CYCODE_UPDATE_{key} "{value}"' not in header:
                        raise RuntimeError(f'Generated identity differs: {label}/{key}')
                if f'#define CYCODE_UPDATE_LEGACY {str(legacy).lower()}' not in header:
                    raise RuntimeError('Legacy metadata must be limited to the historical product/repository')
            if list(build.rglob('*.exe')):
                raise RuntimeError('No-language profile validation unexpectedly produced an executable')
            results.append(dict(scenario=label, accepted=accepted))
    return dict(state='passed', scenarios=results, applicationOrNativeTestLaunched=False,
                nativeCompilation=False, network=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cmake', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.cmake), indent=2))

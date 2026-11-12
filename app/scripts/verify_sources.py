"""Check Python syntax and JSON source data without importing project code or launching tools."""
import ast
import json
from pathlib import Path
import sys

APP = Path(__file__).resolve().parents[1]


def main():
    counts = {'.py': 0, '.json': 0}
    failures = []
    paths = list(APP.rglob('*'))
    companion = APP.parent / 'apps'
    for name in ('CyCodeUpdater', 'CyCodeManager'):
        if (companion / name).is_dir():
            paths.extend((companion / name).rglob('*'))
    for path in sorted(paths):
        if path.suffix not in counts or not path.is_file():
            continue
        if path.is_relative_to(APP / 'help') or path.is_relative_to(APP / 'Documentation'):
            continue
        base = APP if path.is_relative_to(APP) else companion
        if any(part in {'__pycache__', '.git', 'runtime', 'node_modules', '.cache', 'packages', '.venv', 'venv'}
               for part in path.relative_to(base).parts):
            continue
        if not path.resolve().is_relative_to(base):
            failures.append(f'{path}: source file resolves outside app/')
            continue
        try:
            source = path.read_text(encoding='utf-8-sig')
            if path.suffix == '.py':
                ast.parse(source, filename=str(path))
            else:
                json.loads(source)
            counts[path.suffix] += 1
        except (OSError, UnicodeError, SyntaxError, ValueError) as error:
            failures.append(f'{path.relative_to(base)}: {error}')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Source checks passed: {counts[".py"]} Python files, {counts[".json"]} JSON files.')
    print('No project code imported, executable generated, native test launched or download performed.')
    print('This checks source syntax/data only, not C++ compilation or runtime behavior.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

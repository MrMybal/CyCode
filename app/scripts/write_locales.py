"""Compatibility entry: UI translations are owned by CyCodeIDE."""
import argparse
from pathlib import Path
import runpy

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ide-root',type=Path,required=True,help='Explicit trusted CyCodeIDE source checkout')
    args=parser.parse_args()
    root=args.ide_root.resolve()
    if not (root/'AGENTS.md').is_file() or not (root/'app/editor-source.lock.json').is_file():
        raise RuntimeError('Expected the separate CyCodeIDE source repository')
    source=root/'app/scripts/write_locales.py'
    if source.is_symlink() or not source.is_file():
        raise RuntimeError('Expected the IDE-owned locale generator')
    runpy.run_path(str(source),run_name='__main__')

if __name__=='__main__':
    main()

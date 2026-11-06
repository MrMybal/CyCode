"""Copy only a verified CyDoc export to the dedicated publication checkout."""
import argparse
from pathlib import Path
import subprocess
from build import publish
from verify import verify

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('export', type=Path)
    parser.add_argument('repository', type=Path)
    args = parser.parse_args()
    source, repository = args.export.resolve(), args.repository.resolve()
    git = ['git', '-c', 'safe.directory='+repository.as_posix(), '-C', str(repository)]
    top = Path(subprocess.check_output([*git, 'rev-parse', '--show-toplevel'], text=True).strip()).resolve()
    remote = subprocess.check_output([*git, 'remote', 'get-url', 'origin'], text=True).strip()
    if top != repository or remote not in {
        'https://github.com/MrMybal/CyCodeReader.git',
        'git@github.com:MrMybal/CyCodeReader.git',
    }:
        raise RuntimeError('Expected the dedicated CyCode documentation checkout')
    if source.is_relative_to(repository) or repository.is_relative_to(source):
        raise RuntimeError('Export and publication checkout must be separate')
    branch = subprocess.check_output([*git, 'branch', '--show-current'], text=True).strip()
    if branch != 'cycode-docs':
        raise RuntimeError('Expected the generated cycode-docs branch')
    verify(source)
    publish(source, repository/'site')
    verify(repository/'site')

if __name__ == '__main__':
    main()

"""Launch the official MCP adapter with a private SDK data root per connection.

No native operation or independent SDK model is implemented here.
"""
import argparse
import os
from pathlib import Path
import runpy
import sys
import uuid

from scripts.install_core_agents import ordinary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--state', type=Path, required=True)
    args = parser.parse_args(argv)
    home = Path(__file__).resolve().parents[1]
    workspace = ordinary(args.workspace).resolve(strict=True)
    if not workspace.is_dir():
        raise ValueError('Expected an existing project directory')
    state = ordinary(args.state) / 'sessions' / uuid.uuid4().hex
    state.mkdir(parents=True, mode=0o700, exist_ok=False)
    # The persistent project index is shared by this agent's sessions and stays
    # outside the project; an explicit CYCODE_INDEX_DIR keeps precedence.
    os.environ.setdefault('CYCODE_INDEX_DIR', str(ordinary(args.state) / 'index'))
    sys.argv = [str(home / 'app/tool.py'), '--core-workspace', str(workspace),
                '--core-executable', str(home / 'cycode_cli.exe'), '--disable-native-plugins',
                '--data-dir', str(state), 'mcp']
    runpy.run_path(sys.argv[0], run_name='__main__')


if __name__ == '__main__':
    main()

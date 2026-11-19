"""Compatibility launcher for installed Python MCP configurations."""
import argparse
import os
from pathlib import Path
import subprocess
from core_host import find_core


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--state', type=Path, required=True)
    args = parser.parse_args(argv)
    environment = dict(os.environ)
    environment.setdefault('CYCODE_MCP_PROFILE', 'code')  # Existing configurations.
    return subprocess.run([str(find_core()), '--sdk', '--workspace', str(args.workspace),
        '--connection-state', str(args.state), '--disable-plugins', 'mcp'], env=environment).returncode


if __name__ == '__main__':
    raise SystemExit(main())
